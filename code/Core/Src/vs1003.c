/*
 Copyright (C) 2012 Andy Karpov <andy.karpov@gmail.com>
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "vs1003.h"
#include "time.h"
#include "stm32f1xx_hal.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip.h"
#include "ff.h"
#include "common.h"
#include "main.h"

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

extern SPI_HandleTypeDef hspi1;

#define vs1003_chunk_size 32

/****************************************************************************/

// VS1003 SCI Write Command byte is 0x02
#define VS_WRITE_COMMAND 0x02

// VS1003 SCI Read COmmand byte is 0x03
#define VS_READ_COMMAND  0x03

// SCI Registers

#define SCI_MODE            0x0
#define SCI_STATUS          0x1
#define SCI_BASS            0x2
#define SCI_CLOCKF          0x3
#define SCI_DECODE_TIME     0x4
#define SCI_AUDATA          0x5
#define SCI_WRAM            0x6
#define SCI_WRAMADDR        0x7
#define SCI_HDAT0           0x8
#define SCI_HDAT1           0x9
#define SCI_AIADDR          0xa
#define SCI_VOL             0xb
#define SCI_AICTRL0         0xc
#define SCI_AICTRL1         0xd
#define SCI_AICTRL2         0xe
#define SCI_AICTRL3         0xf
#define SCI_num_registers   0xf

// SCI_MODE bits

#define SM_DIFF             0
#define SM_LAYER12          1
#define SM_RESET            2
#define SM_OUTOFWAV         3
#define SM_EARSPEAKER_LO    4
#define SM_TESTS            5
#define SM_STREAM           6
#define SM_EARSPEAKER_HI    7
#define SM_DACT             8
#define SM_SDIORD           9
#define SM_SDISHARE         10
#define SM_SDINEW           11
#define SM_ADPCM            12
#define SM_ADCPM_HP         13
#define SM_LINE_IN          14

const char* internet_radios[] = {
    "http://redir.atmcdn.pl/sc/o2/Eurozet/live/antyradio.livx?audio=5",     //Antyradio
    "http://stream3.polskieradio.pl:8900/",                                 //PR1
    "http://stream3.polskieradio.pl:8902/",                                 //PR2
    "http://stream3.polskieradio.pl:8904/",                                 //PR3
    "http://stream4.nadaje.com:9680/radiokrakow-s3",                        //Kraków
    "http://195.150.20.5/rmf_fm",                                           //RMF
    "http://redir.atmcdn.pl/sc/o2/Eurozet/live/audio.livx?audio=5",         //Zet
    "http://ckluradio.laurentian.ca:88/broadwave.mp3",                      //CKLU
    "http://stream.rcs.revma.com/an1ugyygzk8uv",                            //Radio 357
    "http://stream.rcs.revma.com/ypqt40u0x1zuv",                            //Radio Nowy Swiat
    "http://51.255.8.139:8822/stream"                                       //Radio Pryzmat
};

#define VS_BUFFER_SIZE  1024

static uint8_t vsBuffer[2][VS_BUFFER_SIZE];
static uint16_t vsBuffer_shift = 0;
static uint8_t active_buffer = 0x01;
static uint8_t new_data_needed = 0;

FIL fsrc;
DIR vsdir;

struct tcp_pcb* VS_Socket;

static uri_t uri;
static uint8_t loop_flag = FALSE;
static uint8_t dir_flag = FALSE;

typedef enum {
    STREAM_HOME = 0,
    STREAM_HTTP_BEGIN,
	STREAM_HTTP_WAIT_DNS,
	STREAM_HTTP_OBTAIN_SOCKET,
    STREAM_HTTP_SOCKET_OBTAINED,
	STREAM_HTTP_CONNECT_WAIT,
    STREAM_HTTP_SEND_REQUEST,
    STREAM_HTTP_PROCESS_HEADER,
    STREAM_HTTP_GET_DATA,
    STREAM_FILE_GET_DATA,
    STREAM_HTTP_CLOSE,
    STREAM_HTTP_RECONNECT_WAIT
} StreamState_t;

static StreamState_t StreamState = STREAM_HOME;

typedef enum {
    DO_NOT_RECONNECT = 0,
    RECONNECT_IMMEDIATELY,
    RECONNECT_WAIT_LONG,
    RECONNECT_WAIT_SHORT
} ReconnectStrategy_t;

static ReconnectStrategy_t ReconnectStrategy = DO_NOT_RECONNECT;

// Register names

typedef enum {
    reg_name_MODE = 0,
    reg_name_STATUS,
    reg_name_BASS,
    reg_name_CLOCKF,
    reg_name_DECODE_TIME,
    reg_name_AUDATA,
    reg_name_WRAM,
    reg_name_WRAMADDR,
    reg_name_HDAT0,
    reg_name_HDAT1,
    reg_name_AIADDR,
    reg_name_VOL,
    reg_name_AICTRL0,
    reg_name_AICTRL1,
    reg_name_AICTRL2,
    reg_name_AICTRL3
} register_names_t;

const char * register_names[] =
{
  "MODE",
  "STATUS",
  "BASS",
  "CLOCKF",
  "DECODE_TIME",
  "AUDATA",
  "WRAM",
  "WRAMADDR",
  "HDAT0",
  "HDAT1",
  "AIADDR",
  "VOL",
  "AICTRL0",
  "AICTRL1",
  "AICTRL2",
  "AICTRL3",
};

static void VS1003_startPlaying(void);
static void VS1003_stopPlaying(void);
static inline void await_data_request(void);
static inline void control_mode_on(void);
static inline void control_mode_off(void);
static inline void data_mode_on(void);
static inline void data_mode_off(void);
static uint8_t VS1003_SPI_transfer(uint8_t outB);
static uint8_t is_audio_file (char* name);
static void VS1003_soft_stop (void);
static void dns_cbk(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
static err_t connect_cbk(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t recv_cbk(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

/****************************************************************************/

uint16_t VS1003_read_register(uint8_t _reg) {
  uint16_t result;
  control_mode_on();
  delay_us(1); // tXCSS
  VS1003_SPI_transfer(VS_READ_COMMAND); // Read operation
  VS1003_SPI_transfer(_reg); // Which register
  result = VS1003_SPI_transfer(0xff) << 8; // read high byte
  result |= VS1003_SPI_transfer(0xff); // read low byte
  delay_us(1); // tXCSH
  await_data_request();
  control_mode_off();
  return result;
}

/****************************************************************************/

void VS1003_write_register(uint8_t _reg,uint16_t _value) {
  control_mode_on();
  delay_us(1); // tXCSS
  VS1003_SPI_transfer(VS_WRITE_COMMAND); // Write operation
  VS1003_SPI_transfer(_reg); // Which register
  VS1003_SPI_transfer(_value >> 8); // Send hi byte
  VS1003_SPI_transfer(_value & 0xff); // Send lo byte
  delay_us(1); // tXCSH
  await_data_request();
  control_mode_off();
}

/****************************************************************************/

void VS1003_sdi_send_buffer(const uint8_t* data, int len) {
  int chunk_length;
  
  data_mode_on();
  while ( len ) {
    await_data_request();
    delay_us(3);
    chunk_length = min(len, vs1003_chunk_size);
    len -= chunk_length;
    while ( chunk_length-- ) VS1003_SPI_transfer(*data++);
  }
  data_mode_off();
}

/****************************************************************************/

void VS1003_sdi_send_chunk(const uint8_t* data, int len) {
    if (len > 32) return;
    data_mode_on();
    await_data_request();
    while ( len-- ) VS1003_SPI_transfer(*data++);
    data_mode_off();
}

/****************************************************************************/

void VS1003_sdi_send_zeroes(int len) {
  int chunk_length;  
  data_mode_on();
  while ( len ) {
    await_data_request();
    chunk_length = min(len,vs1003_chunk_size);
    len -= chunk_length;
    while ( chunk_length-- ) VS1003_SPI_transfer(0);
  }
  data_mode_off();
}

/****************************************************************************/

uint8_t VS1003_feed_from_buffer (void) {
    static uint16_t shift = 0;

    if (!HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin)) return 0;

    VS1003_sdi_send_chunk(&vsBuffer[active_buffer][shift], 32);
    shift += 32;
    if (shift >= VS_BUFFER_SIZE) {
        shift = 0;
        active_buffer ^= 0x01;
        new_data_needed = 1;
    }

    return 0;
}

/****************************************************************************/

void VS1003_handle(void) {
	static uint32_t Timer;
	static ip_addr_t server_addr;
	err_t res;

	switch(StreamState)
	{
		case STREAM_HOME:
            //nothing to do here, just wait
            break;

		case STREAM_HTTP_BEGIN:
			//We start with getting address from DNS
			res = dns_gethostbyname(uri.server, &server_addr, dns_cbk, (void*)&server_addr);
			switch (res) {
				case ERR_OK:
					//We already have valid address - proceed
					printf("DNS address already resolved\r\n");
					StreamState = STREAM_HTTP_OBTAIN_SOCKET;
					break;
				case ERR_INPROGRESS:
					//We need to resolve address - wait
					printf("Resolving DNS\r\n");
					StreamState = STREAM_HTTP_WAIT_DNS;
					Timer = millis();
					break;
				case ERR_ARG:
				default:
					//Resolve error - end here
					printf("DNS resolve error\r\n");
					StreamState = STREAM_HOME;
					break;
			}
			break;

		case STREAM_HTTP_WAIT_DNS:
			if ((uint32_t)(millis()-Timer) > 5000) {
				printf("DNS timeout\r\n");
				StreamState = STREAM_HTTP_RECONNECT_WAIT;
				ReconnectStrategy = RECONNECT_WAIT_LONG;
			}
			break;

        case STREAM_HTTP_OBTAIN_SOCKET:
			// Create new socket
            //VS_Socket = TCPOpen((DWORD)&uri.server[0], TCP_OPEN_RAM_HOST, uri.port, TCP_PURPOSE_GENERIC_TCP_CLIENT);
            VS_Socket = tcp_new();

			if(VS_Socket == NULL) {
                StreamState=STREAM_HTTP_RECONNECT_WAIT;
                ReconnectStrategy = RECONNECT_WAIT_LONG;
				break;
            }

			StreamState=STREAM_HTTP_SOCKET_OBTAINED;
			Timer = millis();
			break;

		case STREAM_HTTP_SOCKET_OBTAINED:
			// Connect to temote server
			// TODO: register error callback!
			res = tcp_connect(VS_Socket, &server_addr, uri.port, connect_cbk);
			if (res != ERR_OK) {
				StreamState = STREAM_HTTP_RECONNECT_WAIT;
				ReconnectStrategy = RECONNECT_WAIT_LONG;
				printf("Can't connect to server\r\n");
				break;
			}

            StreamState = STREAM_HTTP_CONNECT_WAIT;
			Timer = millis();
            break;

		case STREAM_HTTP_CONNECT_WAIT:
			if ((uint32_t)(millis()-Timer) > 5000) {
				StreamState = STREAM_HTTP_CLOSE;
				ReconnectStrategy = RECONNECT_WAIT_LONG;
				printf("Connect timeout\r\n");
			}
			break;

        case STREAM_HTTP_SEND_REQUEST:
			// Place the application protocol data into the transmit buffer.

        	tcp_write(VS_Socket, (const void*)"GET ", 4, 0);
			tcp_write(VS_Socket, (const void*)uri.file, strlen(uri.file), 0);
			tcp_write(VS_Socket, (const void*)" HTTP/1.0\r\nHost: ", 17, 0);
			tcp_write(VS_Socket, (const void*)uri.server, strlen(uri.server), 0);
			tcp_write(VS_Socket, (const void*)"\r\nConnection: keep-alive\r\n\r\n", 28, 0);

            printf("Sending headers\r\n");

			// Send the packet
			res = tcp_output(VS_Socket);
			if (res == ERR_OK) {
				Timer = millis();
				vsBuffer_shift = 0;
				StreamState = STREAM_HTTP_PROCESS_HEADER;
				tcp_recv(VS_Socket, recv_cbk);		//Register receive callback
				memset(vsBuffer, 0x00, sizeof(vsBuffer));
			}
			else {
				StreamState = STREAM_HTTP_CLOSE;
				ReconnectStrategy = RECONNECT_WAIT_LONG;
				printf("Can't send HTTP header, reconnecting\r\n");
			}
			break;

        case STREAM_HTTP_PROCESS_HEADER:
			//if(TCPWasReset(VS_Socket))
			{
				StreamState = STREAM_HTTP_CLOSE;
                ReconnectStrategy = RECONNECT_WAIT_LONG;
                printf("Internet radio: socket disconnected - reseting\r\n");
				break;
			}

            //uint16_t to_load = TCPIsGetReady(VS_Socket);
            uint16_t w; // = TCPGetArray(VS_Socket, &vsBuffer[0][vsBuffer_shift], (((VS_BUFFER_SIZE - vsBuffer_shift) >= to_load) ? to_load : (VS_BUFFER_SIZE-vsBuffer_shift)));
            vsBuffer_shift += w;
            if (vsBuffer_shift >= VS_BUFFER_SIZE) {
                vsBuffer_shift = 0;
            }
            vsBuffer[0][vsBuffer_shift] = '\0';
            char* tok = strstr((char*)vsBuffer[0], "\r\n\r\n");
            if (tok) {
                tok[2] = '\0';
                tok[3] = '\0';
                //printf(vsBuffer[0]);
                http_res_t http_result = parse_http_headers((char*)vsBuffer[0], strlen((char*)vsBuffer[0]), &uri);
                switch (http_result) {
                    case HTTP_HEADER_ERROR:
                        printf("Parsing headers error\r\n");
                        ReconnectStrategy = RECONNECT_WAIT_LONG;
                        StreamState = STREAM_HTTP_CLOSE;
                        break;
                    case HTTP_HEADER_OK:
                        printf("It is 200 OK\r\n");
                        Timer = millis();
                        StreamState = STREAM_HTTP_GET_DATA;
                        VS1003_startPlaying();
                        break;
                    case HTTP_HEADER_REDIRECTED:
                        printf("Stream redirected\r\n");
                        ReconnectStrategy = RECONNECT_IMMEDIATELY;
                        StreamState = STREAM_HTTP_CLOSE;
                        break;
                    default:
                        break;
                }
            }

            if ( (uint32_t)(millis()-Timer) > 1000) {
                //There was no data in 1 second - reconnect
                printf("Internet radio: no header timeout - reseting\r\n");
                ReconnectStrategy = RECONNECT_WAIT_LONG;
                StreamState = STREAM_HTTP_CLOSE;
            }
            break;

		case STREAM_HTTP_GET_DATA:
			// Check to see if the remote node has disconnected from us or sent us any application data
			// If application data is available, write it to the UART
			//if(TCPWasReset(VS_Socket))
			{
				StreamState = STREAM_HTTP_CLOSE;
                ReconnectStrategy = RECONNECT_WAIT_LONG;
                printf("Internet radio: socket disconnected - reseting\r\n");
				// Do not break;  We might still have data in the TCP RX FIFO waiting for us
			}

            if ( (uint32_t)(millis()-Timer) > 5000) {
                //There was no data in 5 seconds - reconnect
                printf("Internet radio: no new data timeout - reseting\r\n");
                ReconnectStrategy = RECONNECT_WAIT_LONG;
                StreamState = STREAM_HTTP_CLOSE;
            }

            if (new_data_needed) {
			// Get count of RX bytes waiting
                //to_load = TCPIsGetReady(VS_Socket);
            //    w = TCPGetArray(VS_Socket, &vsBuffer[active_buffer ^ 0x01][vsBuffer_shift], (((VS_BUFFER_SIZE - vsBuffer_shift) >= to_load) ? to_load : (VS_BUFFER_SIZE-vsBuffer_shift)));
                //printf("Received %d bytes from audo stream, Saved in %d buffer at shift %d\r\n", w, (active_buffer ^ 0x01), shift);
                if (w) {
                    //We still receiving new data, so update timer to not reconnect
                    Timer = millis();
                }
                vsBuffer_shift += w;
                if (vsBuffer_shift >= VS_BUFFER_SIZE) {
                    vsBuffer_shift = 0;
                    new_data_needed = 0;
                }
                //printf("New shjift is %d. There is %s need for new data\r\n", shift, new_data_needed ? "still a" : "no");
            }

            VS1003_feed_from_buffer();
			break;

        case STREAM_FILE_GET_DATA:
            if (new_data_needed) {
                unsigned int br;
                //new_data_needed = 0;
                FRESULT res = f_read(&fsrc, &vsBuffer[active_buffer ^ 0x01][vsBuffer_shift], 512, &br);
                if (res == FR_OK) {
                    //printf("%d bytes of data loaded. Buffer %d. Shift %d\r\n", br, (active_buffer ^ 0x01), vsBuffer_shift);
                    vsBuffer_shift += 512;
                    if (vsBuffer_shift >= VS_BUFFER_SIZE) {
                        vsBuffer_shift = 0;
                        new_data_needed = 0;
                    }

                    if (br < 512) {     //end of file
                        if (dir_flag) {
                            VS1003_play_next_audio_file_from_directory();   //it handles loops
                        }
                        else {
                            if (loop_flag) {
                                res = f_lseek(&fsrc, 0);
                                if (res != FR_OK) printf("f_lseek ERROR\r\n");
                            }
                            else {
                                VS1003_stopPlaying();
                                f_close(&fsrc);
                                StreamState = STREAM_HOME;
                            }
                        }
                    }
                }
            }
            VS1003_feed_from_buffer();
            break;

		case STREAM_HTTP_CLOSE:
			// Close the socket so it can be used by other modules
			// For this application, we wish to stay connected, but this state will still get entered if the remote server decides to disconnect
			//TCPDisconnect(VS_Socket);
			//VS_Socket = INVALID_SOCKET;
            VS1003_stopPlaying();
            switch(ReconnectStrategy) {
                case DO_NOT_RECONNECT:
                    StreamState = STREAM_HOME;
                    break;
                case RECONNECT_IMMEDIATELY:
                    StreamState = STREAM_HTTP_BEGIN;
                    break;
                case RECONNECT_WAIT_LONG:
                case RECONNECT_WAIT_SHORT:
                    StreamState = STREAM_HTTP_RECONNECT_WAIT;
                    break;
                default:
                    StreamState = STREAM_HOME;
                    break;
            }
            Timer = millis();
			break;

        case STREAM_HTTP_RECONNECT_WAIT:
            if ( (uint32_t)(millis()-Timer) > ((ReconnectStrategy == RECONNECT_WAIT_LONG) ? 5000 : 1000) ) {
                printf("Internet radio: reconnecting\r\n");
                StreamState = STREAM_HTTP_BEGIN;
            }
            break;
	}
}

/****************************************************************************/

void VS1003_begin(void) {
  // Keep the chip in reset until we are ready
  HAL_GPIO_WritePin(VS_XRST_GPIO_Port, VS_XRST_Pin, 0);

  // The SCI and SDI will start deselected
  HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
  HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);

  // Boot VS1003
  printf("Booting VS1003...\r\n");

  HAL_Delay(1);

  // init SPI slow mode
  //SPI configuration
  //8 bit master mode, CKE=1, CKP=0
  MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_256); //281 kHz

  // release from reset
  HAL_GPIO_WritePin(VS_XRST_GPIO_Port, VS_XRST_Pin, 1);
  
  // Declick: Immediately switch analog off
  VS1003_write_register(SCI_VOL,0xffff); // VOL

  /* Declick: Slow sample rate for slow analog part startup */
  VS1003_write_register(SCI_AUDATA,10);

  HAL_Delay(100);

  /* Switch on the analog parts */
  VS1003_write_register(SCI_VOL,0xfefe); // VOL
  
  printf("VS1003 still booting\r\n");

  VS1003_write_register(SCI_AUDATA,44101); // 44.1kHz stereo

  VS1003_write_register(SCI_VOL,0x2020); // VOL
  
  // soft reset
  VS1003_write_register(SCI_MODE, (1 << SM_SDINEW) | (1 << SM_RESET) );
  HAL_Delay(1);
  await_data_request();
  VS1003_write_register(SCI_CLOCKF,0xB800); // Experimenting with higher clock settings
  HAL_Delay(1);
  await_data_request();

  // Now you can set high speed SPI clock
  //SPI configuration
  ////8 bit master mode, CKE=1, CKP=0
  MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_32);       //2.5 MHz

  printf("VS1003 Set\r\n");
  VS1003_printDetails();
  printf("VS1003 OK\r\n");
}

/****************************************************************************/

void VS1003_setVolume(uint8_t vol) {
  uint16_t value = vol;
  value <<= 8;
  value |= vol;

  VS1003_write_register(SCI_VOL,value); // VOL
}

/****************************************************************************/

void VS1003_playChunk(const uint8_t* data, size_t len) {
  VS1003_sdi_send_buffer(data,len);
}

/****************************************************************************/

void VS1003_print_byte_register(uint8_t reg) {
  char extra_tab = strlen(register_names[reg]) < 5 ? '\t' : 0;
  printf("%02x %s\t%c = 0x%02x\r\n", reg, register_names[reg], extra_tab, VS1003_read_register(reg));
}

/****************************************************************************/

void VS1003_printDetails(void) {
  printf("VS1003 Configuration:\r\n");
  int i = 0;
  while ( i <= SCI_num_registers )
    VS1003_print_byte_register(i++);
}

/****************************************************************************/

void VS1003_loadUserCode(const uint16_t* buf, size_t len) {
  while (len) {
    uint16_t addr = *buf++; len--;
    uint16_t n = *buf++; len--;
    if (n & 0x8000U) { /* RLE run, replicate n samples */
      n &= 0x7FFF;
      uint16_t val = *buf++; len--;
      while (n--) {
	    printf("W %02x: %04x\r\n", addr, val);
        VS1003_write_register(addr, val);
      }
    } else {           /* Copy run, copy n samples */
      while (n--) {
	uint16_t val = *buf++; len--;
	printf("W %02x: %04x\r\n", addr, val);
        VS1003_write_register(addr, val);
      }
    }
  }
}


static inline void await_data_request(void) {
while ( !HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin) );
}

static inline void control_mode_on(void) {
HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);
HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 0);
}

static inline void control_mode_off(void) {
  HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
}

static inline void data_mode_on(void) {
HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 0);
}

static inline void data_mode_off(void) {
  HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);
}

static void VS1003_startPlaying(void) {
VS1003_sdi_send_zeroes(10);
}

static void VS1003_stopPlaying(void) {
  VS1003_sdi_send_zeroes(2048);
  memset(vsBuffer, 0x00, sizeof(vsBuffer));
}

static uint8_t VS1003_SPI_transfer(uint8_t outB) {
uint8_t answer;

HAL_SPI_TransmitReceive(&hspi1, &outB, &answer, 1, HAL_MAX_DELAY);
return answer;
}

static uint8_t is_audio_file (char* name) {
  if (strstr(name, ".MP3") || strstr(name, ".WMA") || strstr(name, ".MID") || strstr(name, ".mp3") || strstr(name, ".wma") || strstr(name, ".mid")) {
	  return 1;
  }
  return 0;
}

/* This is needed for directory playing - it stops playing
and closes current file, but doesn't close directory and
leaves flag unchanged */

static void VS1003_soft_stop (void) {
  //Can be used only if it is actually playing from file
  if (StreamState == STREAM_FILE_GET_DATA) {
	  f_close(&fsrc);
	  VS1003_stopPlaying();
	  StreamState = STREAM_HOME;
  }
}

static void dns_cbk(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  ip_addr_t* dst = (ip_addr_t*)callback_arg;

  if (StreamState != STREAM_HTTP_WAIT_DNS) return;
  printf("DNS %s resolved\r\n", name);
  *dst = *ipaddr;
  StreamState = STREAM_HTTP_OBTAIN_SOCKET;
}

static err_t connect_cbk(void *arg, struct tcp_pcb *tpcb, err_t err) {
	if (StreamState != STREAM_HTTP_CONNECT_WAIT) {
		return ERR_OK;	//What value should be used?
	}
	printf("Connected successfully\r\n");
	StreamState = STREAM_HTTP_SEND_REQUEST;

	return ERR_OK;
}

static err_t recv_cbk(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	if (p == NULL) {
		//connection lost

	}
	//handle received data here
	tcp_recved(VS_Socket, p->tot_len);
}


void VS1003_play_next_audio_file_from_directory (void) {
  FILINFO info;
  char buf[257];

  if(!dir_flag) return;       //currently we are not playing directory

  while (f_readdir(&vsdir, &info) == FR_OK) {
	  if (!info.fname[0]) {           //Empty string - end of directory
		  if (loop_flag) {
			  f_rewinddir(&vsdir);
		  }
		  else {
			  VS1003_stop();          //It handles closing dir and resetting dir_flag
		  }
	  }
	  else {
		  if (is_audio_file(info.fname)) {
			  snprintf(buf, sizeof(buf)-1, "%s/%s", uri.server, info.fname);
			  VS1003_soft_stop();
			  VS1003_play_file(buf);
			  return;
		  }
	  }
  }

  return;
}

/*Always call VS1003_stop() before calling that function*/
void VS1003_play_http_stream(const char* url) {
  if (StreamState != STREAM_HOME) return;

  if (parse_url(url, strlen(url), &uri)) {
	  StreamState = STREAM_HTTP_BEGIN;
	  ReconnectStrategy = RECONNECT_WAIT_SHORT;
  }
  else {
	  StreamState = STREAM_HOME;
	  ReconnectStrategy = DO_NOT_RECONNECT;
  }
  VS1003_startPlaying();
}

void VS1003_play_next_http_stream_from_list(void) {
  static int ind = 0;

  ind++;
  if (ind >= sizeof(internet_radios)/sizeof(const char*)) ind=0;
  VS1003_stop();
  VS1003_play_http_stream(internet_radios[ind]);
}

/*Always call VS1003_stop() or VS1003_soft_stop() before calling that function*/
void VS1003_play_file (char* url) {
  if (StreamState != STREAM_HOME) return;

  FRESULT res = f_open(&fsrc, url, FA_READ);
  if (res != FR_OK) {
	  printf("f_open error code: %i\r\n", res);
	  StreamState = STREAM_HOME;
	  return;
  }

  StreamState = STREAM_FILE_GET_DATA;
  VS1003_startPlaying();         //Start playing song
}


void VS1003_play_dir (const char* url) {
  FRESULT res;

  res = f_opendir(&vsdir, url);
  if (res != FR_OK) {
	  printf("f_opendir error code: %i\r\n", res);
	  return;
  }
  dir_flag = TRUE;
  strncpy(uri.server, url, sizeof(uri.server)-1);		//we use uri.server to store current directory path
  VS1003_play_next_audio_file_from_directory();
}

void VS1003_stop(void) {
  //Can be stopped only if it is actually playing
  switch (StreamState) {
	  case STREAM_HTTP_BEGIN:
	  case STREAM_HTTP_SOCKET_OBTAINED:
	  case STREAM_HTTP_SEND_REQUEST:
	  case STREAM_HTTP_PROCESS_HEADER:
	  case STREAM_HTTP_GET_DATA:
		  //if(VS_Socket != INVALID_SOCKET) {
		  //    TCPDisconnect(VS_Socket);
		  //    VS_Socket = INVALID_SOCKET;
		  //}
		  break;
	  case STREAM_FILE_GET_DATA:
		  f_close(&fsrc);
		  if (dir_flag) {
			  f_closedir(&vsdir);
			  dir_flag = FALSE;
		  }
		  break;
	  default:
		  return;
		  break;
  }
  VS1003_stopPlaying();
  StreamState = STREAM_HOME;
}

void VS1003_setLoop(uint8_t val) {
  loop_flag = val;
}

uint8_t VS1003_getLoop(void) {
  return loop_flag;
}
