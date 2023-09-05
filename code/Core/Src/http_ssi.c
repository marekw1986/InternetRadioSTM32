/*
 * http_ssi.c
 *
 *  Created on: 25 sty 2023
 *      Author: marek
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lwip.h"
#include "lwip/tcp.h"
#include "lwip/apps/httpd.h"
#include "vs1003.h"
#include "main.h"
#include "common.h"
#include "cmsis_os.h"

enum {PLAY_UNKNOWN, PLAY_OK, PLAY_INVALID_TOKEN, PLAY_INVALID_URL, PLAY_INVALID_SRC};
typedef enum {DIR_CGI_PRINT_ROOT, DIR_CGI_PRINT_STREAMS, DIR_CGI_PRINT_FS, DIR_CGI_ERROR} dircgiprint_t;

dircgiprint_t dirCgiPrint;

extern osMessageQId vsQueueHandle;

char token[9];
char parent[256];

const char *cgiPlayHandler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char *cgiDirHandler(int iIndex, int iNumparams, char *pcParam[], char *pcValue[]);
uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen, u16_t current_tag_part, u16_t *next_tag_part);

char const* TAGCHAR[] = {"token", "parent", "dirs", "files", "uptime", "time", "cpufreq"};
char const** TAGS=TAGCHAR;

uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen, u16_t current_tag_part, u16_t *next_tag_part) {
	u16_t respLen = 0;

	switch(iIndex) {
		case 0:	//token
			for (int i=0; i<8; i++) { token[i] = 'a' + (rand() % 26); }
			token[8] = '\0';
			strcpy(pcInsert, token);
			respLen = strlen(pcInsert);
		break;

		case 1:	//parent
			if (dirCgiPrint == DIR_CGI_PRINT_ROOT) {
				strcpy(pcInsert, "root");
				respLen = strlen(pcInsert);
			}
			else if (dirCgiPrint == DIR_CGI_PRINT_STREAMS) {
				strcpy(pcInsert, "streams");
				respLen = strlen(pcInsert);
			}
			else if (dirCgiPrint == DIR_CGI_PRINT_FS) {
				strcpy(pcInsert, parent);
				respLen = strlen(pcInsert);
			}
		break;

		case 2:	//dirs
			strcpy(pcInsert, "\"dirs\"");
			respLen = strlen(pcInsert);
		break;

		case 3: ;	//files
			char* streamurl;
			streamurl = get_station_url_from_file(1, NULL, 0);
			if (streamurl != NULL) {
				respLen = snprintf(pcInsert, iInsertLen-1, "\"%s\"", streamurl);
			}
		break;

		case 4:; //uptime
		    u16_t days, hours, minutes;
		    u32_t seconds;

		    seconds = millis()/1000;
		    days = seconds / 86400;
		    seconds -= days * 86400;
		    hours = seconds / 3600;
		    seconds -= hours * 3600;
		    minutes = seconds / 60;
		    seconds -= minutes * 60;
		    if (days) {
		        respLen = snprintf(pcInsert, iInsertLen-1, "%d dni, %02d:%02d:%02lu", days, hours, minutes, seconds);
		    }
		    else {
		    	respLen = snprintf(pcInsert, iInsertLen-1, "%02d:%02d:%02lu", hours, minutes, seconds);
		    }
		break;

		case 5:; //time
			time_t newTimestamp = rtc_get_timestamp();
			struct tm* time_tm = localtime(&newTimestamp);
			if (time_tm) { respLen = snprintf(pcInsert, iInsertLen-1, "%s\r\n", asctime(time_tm)); }
		break;

		case 6: //cpufreq
			respLen = snprintf(pcInsert, iInsertLen-1, "%d", HAL_RCC_GetHCLKFreq());
		break;

		default:
		break;
	}
	return respLen;
}


const tCGI PLAY_CGI = {"/ui/play.cgi", cgiPlayHandler};
const tCGI DIR_CGI = {"/ui/dir.cgi", cgiDirHandler};

const char *cgiDirHandler(int iIndex, int iNumparams, char *pcParam[], char *pcValue[]) {

	if (iIndex == 0) {

		dirCgiPrint = DIR_CGI_ERROR;

		for (int i=0; i<iNumparams; i++) {
			if (strncmp(pcParam[i], "url", 4) == 0) {
				if (strncmp(pcValue[i], "root", 5) == 0) {
					dirCgiPrint = DIR_CGI_PRINT_ROOT;
				}
				else if (strncmp(pcValue[i], "streams", 8) == 0) {
					dirCgiPrint = DIR_CGI_PRINT_STREAMS;
				}
				else {
					dirCgiPrint = DIR_CGI_PRINT_FS;
					strncpy(parent, pcValue[i], sizeof(parent)-1);
				}
			}
		}
	}

	return "/ui/dir.shtml";
}

const char *cgiPlayHandler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
	enum {PLAY_SRC_STREAM, PLAY_SRC_FILE, PLAY_SRC_DIR};

	static uint8_t to_send = 0;
	//int playSrc = PLAY_SRC_STREAM;	//Stream is default

	if (iIndex == 0) {
		for (int i=0; i<iNumParams; i++) {
			if (strncmp(pcParam[i], "token", 6) == 0) {
				//Needs to be implemented
			}
			else if (strncmp(pcParam[i], "url", 4) == 0) {
				printf("play.cgi received URL: %s\r\n", pcValue[i]);
				if (strlen(pcValue[i]) == 0) {
					//Empty url, stop playing
					VS1003_send_cmd_thread_safe(VS_MSG_STOP, 0);
					return "/ui/player.shtml";
				}
				else {
					if (strncmp(pcValue[i], "next", 5) == 0) {
						VS1003_send_cmd_thread_safe(VS_MSG_NEXT, 0);
						return "/ui/player.shtml";
					}
					else {

					}
				}
			}
			else if (strncmp(pcParam[i], "src", 4) == 0) {
				if (strncmp(pcValue[i], "stream", 7) == 0) {}
				else if (strncmp(pcValue[i], "file", 5) == 0) {}
				else if (strncmp(pcValue[i], "dir", 4) == 0) {}
				else {
					//Invalid src TODO
				}
			}
			else if (strncmp(pcParam[i], "id", 3) == 0) {
				uint16_t id = strtol(pcValue[i], NULL, 10);
				VS1003_send_cmd_thread_safe(VS_MSG_PLAY_BY_ID, id);
			}
			else if (strncmp(pcParam[i], "vol", 4) == 0) {
				uint8_t volume = strtol(pcValue[i], NULL, 10);
				VS1003_send_cmd_thread_safe(VS_MSG_SET_VOL, volume);
			}
		}
	}
	return "/ui/player.shtml";
}

void init_http_server (void) {
	http_set_ssi_handler(ssi_handler, (char const**) TAGS, 7);
	http_set_cgi_handlers(&PLAY_CGI, 1);
	http_set_cgi_handlers(&DIR_CGI, 1);
}
