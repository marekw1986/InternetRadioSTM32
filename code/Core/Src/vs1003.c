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
#include <math.h>
#include <tm.h>

#include "vs1003_low_level.h"
#include "cmsis_os.h"
#include "vs1003.h"
#include "stm32f1xx_hal.h"
#include "lwip/tcp.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip.h"
#include "ff.h"
#include "spiram.h"
#include "common.h"
#include "http_header_parser.h"
#include "stream_list.h"
#include "main.h"
#ifdef USE_LCD_UI
#include "ui.h"
#endif

#define RECONNECT_LIMIT 3

extern osMessageQId vsQueueHandle;

FIL fsrc;
DIR vsdir;
static uint8_t ReconnectLimit = RECONNECT_LIMIT;
static int current_stream_ind = 1;

int sock;

static uri_t uri;
static uint8_t loop_flag = FALSE;
static uint8_t dir_flag = FALSE;
static uint8_t last_volume = 0;

static StreamState_t StreamState = STREAM_HOME;

typedef enum {
    DO_NOT_RECONNECT = 0,
    RECONNECT_IMMEDIATELY,
    RECONNECT_WAIT_LONG,
    RECONNECT_WAIT_SHORT
} ReconnectStrategy_t;

static ReconnectStrategy_t ReconnectStrategy = DO_NOT_RECONNECT;

static void VS1003_startPlaying(void);
static void VS1003_stopPlaying(void);
static uint8_t is_audio_file (char* name);
static void VS1003_soft_stop (void);
static void VS1003_handle_end_of_file (void);

void VS1003_init(void) {
    VS1003_low_level_init();
}

void VS1003_handle(void) {
	struct hostent* remoteHost;
	struct sockaddr_in addr;
	static uint32_t timer = 0;
	int ret;
	uint16_t w = 0;
	FRESULT fres;
	unsigned int br;
	uint8_t data[32];
	vs1003cmd_t rcv;

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
            http_prepare_parser();

            StreamState = STREAM_HTTP_PROCESS_HEADER;
			break;

        case STREAM_HTTP_PROCESS_HEADER:;
			w = lwip_recv(sock, data, 32, 0);
			if (w > 0) {
				http_res_t http_result = http_parse_headers((char*)data, w, &uri);
				switch (http_result) {
					case HTTP_HEADER_ERROR:
						printf("Parsing headers error, code: %d\r\n", http_get_err_code());
						http_release_parser();
						ReconnectStrategy = DO_NOT_RECONNECT;
						StreamState = STREAM_HTTP_CLOSE;
						break;
					case HTTP_HEADER_OK:
						printf("It is 200 OK\r\n");
						http_release_parser();
						timer = millis();
						StreamState = STREAM_HTTP_FILL_BUFFER;     //STREAM_HTTP_GET_DATA
						VS1003_startPlaying();
						break;
					case HTTP_HEADER_REDIRECTED:
						printf("Stream redirected\r\n");
						http_release_parser();
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
                	ReconnectLimit--;
                    if (ReconnectLimit > 0) {
                        StreamState = STREAM_HTTP_BEGIN;
                    }
                    else {
                    	StreamState = STREAM_HOME;
                    	printf("Reconnect limit reached\r\n");
                    }
                    StreamState = STREAM_HTTP_BEGIN;
                    break;
                case RECONNECT_WAIT_LONG:
                case RECONNECT_WAIT_SHORT:
                	ReconnectLimit--;
                    if (ReconnectLimit > 0) {
                        StreamState = STREAM_HTTP_RECONNECT_WAIT;
                    }
                    else {
                    	StreamState = STREAM_HOME;
                    	printf("Reconnect limit reached\r\n");
                    }
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

	if (xQueueReceive(vsQueueHandle, &rcv, 5) == pdTRUE) {
		printf("Received command %d from queque\r\n", rcv.cmd);
		switch(rcv.cmd) {
			case VS_MSG_NEXT:
				VS1003_play_next();
				break;
			case VS_MSG_STOP:
				VS1003_stop();
				break;
			case VS_MSG_PLAY_BY_ID:
				VS1003_play_http_stream_by_id(rcv.param);
				break;
			case VS_MSG_SET_VOL:
				if ( (rcv.param >= 0) && (rcv.param <= 100) ) {
					VS1003_setVolume(rcv.param);
				}
				break;
			default:
				break;
		}
	}
}

void VS1003_setVolume(uint8_t vol) {
	if ((vol < 1) || (vol > 100)) return;
	int x = log10f(vol)*1000;
	uint8_t new_reg_value = map(x, 0, 2000, 0xFE, 0x00);//vol;
	last_volume = vol;
	uint16_t value = new_reg_value;
	value <<= 8;
	value |= new_reg_value;

	VS1003_write_register(SCI_VOL,value); // VOL

	#ifdef USE_LCD_UI
	ui_update_volume();
	#endif
}

uint8_t VS1003_getVolume(void) {
    return last_volume;
}

void VS1003_playChunk(const uint8_t* data, size_t len) {
  VS1003_sdi_send_buffer(data,len);
}

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
        case STREAM_HOME:
        case STREAM_HTTP_FILL_BUFFER:
        case STREAM_HTTP_GET_DATA:
            VS1003_stop();
            VS1003_play_next_http_stream_from_list();
            break;
        default:
            break;
    }
}

void VS1003_play_prev(void) {
    switch (StreamState) {
        case STREAM_FILE_FILL_BUFFER:
        case STREAM_FILE_GET_DATA:
            if (dir_flag) {
                //TODO: This need to be implemented
                //VS1003_play_prev_audio_file_from_directory();
            }
            break;
        case STREAM_HOME:
        case STREAM_HTTP_FILL_BUFFER:
        case STREAM_HTTP_GET_DATA:
            VS1003_stop();
            VS1003_play_prev_http_stream_from_list();
            break;
        default:
            break;
    }
}

static void VS1003_startPlaying(void) {
	VS1003_sdi_send_zeroes(10);
}

static void VS1003_stopPlaying(void) {
  spiram_clear_ringbuffer();
  VS1003_sdi_send_zeroes(2048);
  spiram_clear_ringbuffer();
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
    if ((StreamState == STREAM_HOME) || (StreamState == STREAM_HTTP_RECONNECT_WAIT)) {
        if (parse_url(url, strlen(url), &uri)) {
            StreamState = STREAM_HTTP_BEGIN;
            ReconnectStrategy = RECONNECT_WAIT_SHORT;
            ReconnectLimit = RECONNECT_LIMIT;
        }
        else {
            StreamState = STREAM_HOME;
            ReconnectStrategy = DO_NOT_RECONNECT;
        }
//        mediainfo_type_set(MEDIA_TYPE_STREAM);
        VS1003_startPlaying();
    }
}

void VS1003_play_http_stream_by_id(uint16_t id) {
	char working_buffer[512];
	char* url = get_station_url_from_file(id, working_buffer, sizeof(working_buffer), NULL, 0);
	if (url) {
		VS1003_stop();
		VS1003_play_http_stream(url);
		current_stream_ind = id;
	}
}

void VS1003_play_next_http_stream_from_list(void) {
	char working_buffer[512];
	char* url = get_station_url_from_file(current_stream_ind, working_buffer, sizeof(working_buffer), NULL, 0);
    if (url == NULL) {
        //Function returned NULL, there is no stream with such ind
        //Try again from the beginning
    	current_stream_ind = 1;
    	url = get_station_url_from_file(current_stream_ind, working_buffer, sizeof(working_buffer), NULL, 0);
        if (url == NULL) return;
    }
    VS1003_stop();
//    mediainfo_title_set(name);
    VS1003_play_http_stream(url);
}

void VS1003_play_prev_http_stream_from_list(void) {
	char working_buffer[512];

    current_stream_ind--;
    if (current_stream_ind < 1) { current_stream_ind = get_max_stream_id(); }
    char* url = get_station_url_from_file(current_stream_ind, working_buffer, sizeof(working_buffer), NULL, 0);
    if (url == NULL) return;
    VS1003_stop();
//    mediainfo_title_set(name);
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
  StreamState = STREAM_HOME;
}

void VS1003_setLoop(uint8_t val) {
  loop_flag = val;
}

uint8_t VS1003_getLoop(void) {
  return loop_flag;
}

StreamState_t VS1003_getStreamState(void) {
    return StreamState;
}

void VS1003_send_cmd_thread_safe(uint8_t cmd, uint16_t param) {
	vs1003cmd_t command;
	command.cmd = cmd;
	command.param = param;
	if (xQueueSend(vsQueueHandle, (void*)&command, portMAX_DELAY)) {
		printf("Sending thread safe command to VS1003: %d\r\n", command.cmd);
	}
}
