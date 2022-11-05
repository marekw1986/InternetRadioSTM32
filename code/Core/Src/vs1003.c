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
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip.h"
#include "ff.h"
#include "spiram.h"
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

FIL fsrc;
DIR vsdir;

int sock;

static uri_t uri;
static uint8_t loop_flag = FALSE;
static uint8_t dir_flag = FALSE;

typedef enum {
    STREAM_HOME = 0,
    STREAM_HTTP_BEGIN,
    STREAM_HTTP_PROCESS_HEADER,
	STREAM_HTTP_FILL_BUFFER,
    STREAM_HTTP_GET_DATA,
	STREAM_FILE_FILL_BUFFER,
    STREAM_FILE_GET_DATA,
	STREAM_FILE_PLAY_REST,
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

typedef enum {
    FEED_RET_NO_DATA_NEEDED = 0,
    FEED_RET_OK,
    FEED_RET_BUFFER_EMPTY
} feed_ret_t;

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
static void VS1003_handle_end_of_file (void);
//static void dns_cbk(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
static feed_ret_t VS1003_feed_from_buffer (void);;

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

static feed_ret_t VS1003_feed_from_buffer (void) {
    uint8_t data[32];

    if (!HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin)) return FEED_RET_NO_DATA_NEEDED;
    do {
        if (spiram_get_num_of_bytes_in_ringbuffer() < 32) return FEED_RET_BUFFER_EMPTY;

        uint16_t w = spiram_read_array_from_ringbuffer(data, 32);
        if (w == 32) VS1003_sdi_send_chunk(data, 32);
        asm("nop");
        asm("nop");
        asm("nop");
    } while(HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin));

    return FEED_RET_OK;
}

/****************************************************************************/

void VS1003_handle(void) {
	struct hostent* remoteHost;
	struct sockaddr_in addr;
	static uint32_t timer = 0;
	int ret;
	uint16_t w = 0;
	FRESULT fres;
	unsigned int br;
	uint8_t data[32];

	switch(StreamState)
	{
		case STREAM_HOME:
            //nothing to do here, just wait
            break;

		case STREAM_HTTP_BEGIN:
			printf("Starting new connection\r\n)");
			//clear circular buffer
			spiram_clear_ringbuffer();
			memset(&addr, 0x00, sizeof(struct sockaddr_in));

			//We start with getting address from DNS
			remoteHost = lwip_gethostbyname(uri.server);
			if (remoteHost == NULL) {
				printf("Can't resolve address %s\r\n", uri.server);
				StreamState = STREAM_HOME;
				break;
			}
			printf("Address %s has been resolved\r\n", uri.server);
			if (remoteHost->h_addrtype != AF_INET) {
				printf("It is not AF_INET address\r\n");
				StreamState = STREAM_HOME;
				break;
			}
			printf("It is AF_INET\r\n");
			addr.sin_family = AF_INET;
			addr.sin_port = htons(uri.port);
			addr.sin_addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];
			printf("IPv4 Address: %s\r\n", inet_ntoa(addr.sin_addr));

			//Acquire socket
        	sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(sock == -1) {
				printf("Can't acquire socket. Errno: %d\r\n", errno);
                StreamState=STREAM_HOME;
				break;
            }
			printf("Socket acquired\r\n");

			//Connect
			ret = lwip_connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr));
			if (ret == -1) {
				printf("Can't connect. Errno: %d\r\n", errno);
				StreamState = STREAM_HOME;
				break;
			}
			printf("Connected\r\n");

			//Send request
        	ret = lwip_send(sock, (void*)"GET ", 4, 0);
        	printf("First ret: %d\r\n", ret);
			ret = lwip_send(sock, (void*)uri.file, strlen(uri.file), 0);
			printf("Second ret: %d\r\n", ret);
			ret = lwip_send(sock, (void*)" HTTP/1.0\r\nHost: ", 17, 0);
			printf("Third ret: %d\r\n", ret);
			ret = lwip_send(sock, (void*)uri.server, strlen(uri.server), 0);
			printf("Fourth ret: %d\r\n", ret);
			ret = lwip_send(sock, (void*)"\r\nConnection: keep-alive\r\n\r\n", 28, 0);
			printf("Fifth ret: %d\r\n", ret);
            printf("Sending headers\r\n");

            StreamState = STREAM_HTTP_PROCESS_HEADER;
			break;

        case STREAM_HTTP_PROCESS_HEADER:;
			w = lwip_recv(sock, data, 32, 0);
			if (w > 0) {
				http_res_t http_result = parse_http_headers((char*)data, w, &uri);
				switch (http_result) {
					case HTTP_HEADER_ERROR:
						printf("Parsing headers error\r\n");
						ReconnectStrategy = RECONNECT_WAIT_LONG;
						StreamState = STREAM_HTTP_CLOSE;
						break;
					case HTTP_HEADER_OK:
						printf("It is 200 OK\r\n");
						timer = millis();
						StreamState = STREAM_HTTP_FILL_BUFFER;     //STREAM_HTTP_GET_DATA
						VS1003_startPlaying();
						break;
					case HTTP_HEADER_REDIRECTED:
						printf("Stream redirected\r\n");
						ReconnectStrategy = RECONNECT_IMMEDIATELY;
						StreamState = STREAM_HTTP_CLOSE;
						break;
					case HTTP_HEADER_IN_PROGRESS:
						break;
					default:
						break;
				}
			}
            break;

        case STREAM_HTTP_FILL_BUFFER:
            while (spiram_get_remaining_space_in_ringbuffer() > 128) {
            	w = lwip_recv(sock, data, 32, 0);
            	if (w > 0) {
            		timer = millis();
            		spiram_write_array_to_ringbuffer(data, w);
            	}
                if ( (uint32_t)(millis()-timer) > 5000) {
                    //There was no data in 5 seconds - close
                    printf("Internet radio: no new data timeout - closing\r\n");
                    ReconnectStrategy = DO_NOT_RECONNECT;
                    StreamState = STREAM_HTTP_CLOSE;
                    break;
                }
            }
            if (StreamState == STREAM_HTTP_CLOSE) break;
			printf("Buffer filled\r\n");
			timer = millis();
			StreamState = STREAM_HTTP_GET_DATA;
			break;

		case STREAM_HTTP_GET_DATA:
            while (spiram_get_remaining_space_in_ringbuffer() > 128) {
            	w = lwip_recv(sock, data, 32, 0);
            	if (w > 0) {
            		timer = millis();
            		spiram_write_array_to_ringbuffer(data, w);
            	}
                if ( (uint32_t)(millis()-timer) > 5000) {
                    //There was no data in 5 seconds - close
                    printf("Internet radio: no new data timeout - closing\r\n");
                    ReconnectStrategy = DO_NOT_RECONNECT;
                    StreamState = STREAM_HTTP_CLOSE;
                }
            	if (HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin)) break;
            }
            if (StreamState == STREAM_HTTP_CLOSE) break;
            if (VS1003_feed_from_buffer() == FEED_RET_BUFFER_EMPTY) {
                StreamState = STREAM_HTTP_FILL_BUFFER;
                timer = millis();
                break;
            }
			break;

        case STREAM_FILE_FILL_BUFFER:
           while (spiram_get_remaining_space_in_ringbuffer() > 128) {
               fres = f_read(&fsrc, data, 32, &br);
               if (fres == FR_OK) {
                   if (br) { spiram_write_array_to_ringbuffer(data, br); }
                   if (br < 32) {  //end of file
                	   StreamState = STREAM_FILE_PLAY_REST;
                	   break;
                   }
               }
           }
           if (StreamState == STREAM_FILE_PLAY_REST) break;
           StreamState = STREAM_FILE_GET_DATA;
           break;

       case STREAM_FILE_GET_DATA:
           while (spiram_get_remaining_space_in_ringbuffer() > 128) {
			   fres = f_read(&fsrc, data, 32, &br);
			   if ( fres == FR_OK ) {
				   if (br) { spiram_write_array_to_ringbuffer(data, 32); }
				   if (br < 32) {     //end of file
					   StreamState = STREAM_FILE_PLAY_REST;
					   break;
				   }
			   }
			   if (HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin)) break;
           }
           if (StreamState == STREAM_FILE_PLAY_REST) break;
           if (VS1003_feed_from_buffer() == FEED_RET_BUFFER_EMPTY) {
               //buffer empty
               StreamState = STREAM_FILE_FILL_BUFFER;
           }
           break;

        case STREAM_FILE_PLAY_REST:
            if (VS1003_feed_from_buffer() == FEED_RET_BUFFER_EMPTY) {
                //buffer empty
            	VS1003_handle_end_of_file();
            }
        	break;

		case STREAM_HTTP_CLOSE:
			// Close the socket so it can be used by other modules
			// For this application, we wish to stay connected, but this state will still get entered if the remote server decides to disconnect
			lwip_close(sock);
			printf("Successfully disconnected\r\n");
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
            timer = millis();
			break;

        case STREAM_HTTP_RECONNECT_WAIT:
            if ( (uint32_t)(millis()-timer) > ((ReconnectStrategy == RECONNECT_WAIT_LONG) ? 5000 : 1000) ) {
                printf("Internet radio: reconnecting\r\n");
                StreamState = STREAM_HTTP_BEGIN;
            }
            break;

        default:
        	StreamState = STREAM_HOME;
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

  osDelay(1);

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

  osDelay(100);

  /* Switch on the analog parts */
  VS1003_write_register(SCI_VOL,0xfefe); // VOL
  
  printf("VS1003 still booting\r\n");

  VS1003_write_register(SCI_AUDATA,44101); // 44.1kHz stereo

  VS1003_write_register(SCI_VOL,0x2020); // VOL
  
  // soft reset
  VS1003_write_register(SCI_MODE, (1 << SM_SDINEW) | (1 << SM_RESET) );
  osDelay(1);
  await_data_request();
  VS1003_write_register(SCI_CLOCKF,0xF800); // Experimenting with highest clock settings
  osDelay(1);
  await_data_request();

  // Now you can set high speed SPI clock
  //SPI configuration
  ////8 bit master mode, CKE=1, CKP=0
  MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_8);       //9 MHz

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

void VS1003_play_next(void) {
    switch (StreamState) {
        case STREAM_FILE_FILL_BUFFER:
        case STREAM_FILE_GET_DATA:
            if (dir_flag) {
                VS1003_play_next_audio_file_from_directory();
            }
            break;
        case STREAM_HOME:	//TEMP
        case STREAM_HTTP_FILL_BUFFER:
        case STREAM_HTTP_GET_DATA:
            VS1003_stop();
            VS1003_play_next_http_stream_from_list();
            break;
        default:
            break;
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
  spiram_clear_ringbuffer();
  VS1003_sdi_send_zeroes(2048);
  spiram_clear_ringbuffer();
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
	if ( StreamState == STREAM_FILE_GET_DATA || StreamState == STREAM_FILE_PLAY_REST ) {
		f_close(&fsrc);
		VS1003_stopPlaying();
		StreamState = STREAM_HOME;
	}
}

static void VS1003_handle_end_of_file (void) {
    FRESULT res;

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

//static void dns_cbk(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
//	ip_addr_t* dst = (ip_addr_t*)callback_arg;
//
//	if (StreamState != STREAM_HTTP_WAIT_DNS) return;
//	printf("DNS %s resolved\r\n", name);
//	*dst = *ipaddr;
//	StreamState = STREAM_HTTP_OBTAIN_SOCKET;
//}


void VS1003_play_next_audio_file_from_directory (void) {
  FILINFO info;
  char buf[300];

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
	  printf("URL %s parsed sucessfully\r\n", url);
	  StreamState = STREAM_HTTP_CLOSE;
	  ReconnectStrategy = RECONNECT_WAIT_SHORT;
  }
  else {
	  printf("URL parsing error\r\n");
	  StreamState = STREAM_HOME;
	  ReconnectStrategy = DO_NOT_RECONNECT;
  }
  VS1003_startPlaying();
}

void VS1003_play_next_http_stream_from_list(void) {
    static int ind = 1;

    char* url = get_station_url_from_file(ind, NULL, 0);
    if (url == NULL) {
        //Function returned NULL, there is no stream with such ind
        //Try again from the beginning
        ind = 1;
        url = get_station_url_from_file(ind, NULL, 0);
        if (url == NULL) return;
    }
    else {
        ind++;
    }
    VS1003_stop();
    VS1003_play_http_stream(url);
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
	  case STREAM_HTTP_PROCESS_HEADER:
	  case STREAM_HTTP_FILL_BUFFER:
	  case STREAM_HTTP_GET_DATA:
//		  lwip_close(sock);
		  //printf("Connection closed\r\n");
		  StreamState = STREAM_HOME;
		  break;
	  case STREAM_FILE_FILL_BUFFER:
	  case STREAM_FILE_GET_DATA:
	  case STREAM_FILE_PLAY_REST:
		  f_close(&fsrc);
		  if (dir_flag) {
			  f_closedir(&vsdir);
			  dir_flag = FALSE;
		  }
		  StreamState = STREAM_HOME;
		  break;
	  default:
		  return;
		  break;
  }
  VS1003_stopPlaying();
}

void VS1003_setLoop(uint8_t val) {
  loop_flag = val;
}

uint8_t VS1003_getLoop(void) {
  return loop_flag;
}
