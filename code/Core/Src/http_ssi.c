/*
 * http_ssi.c
 *
 *  Created on: 25 sty 2023
 *      Author: marek
 */

#include <stdio.h>
#include <string.h>

#include "lwip.h"
#include "lwip/tcp.h"
#include "lwip/apps/httpd.h"
#include "vs1003.h"
#include "main.h"
#include "cmsis_os.h"

extern osMessageQId vsQueueHandle;

char token[9];

enum {PLAY_UNKNOWN, PLAY_OK, PLAY_INVALID_TOKEN, PLAY_INVALID_URL, PLAY_INVALID_SRC};

const char *cgiPlayHandler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen);

char const* TAGCHAR[] = {"token"};
char const** TAGS=TAGCHAR;

uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen) {
	switch(iIndex) {
		case 0:
		for (int i=0; i<8; i++) { token[i] = 'a' + (rand() % 26); }
		token[8] = '\0';
		strcpy(pcInsert, token);
		return strlen(pcInsert);
		break;

		default:
		break;
	}
	return 0;
}


const tCGI PLAY_CGI = {"/ui/play.cgi", cgiPlayHandler};

const char *cgiPlayHandler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
	enum {PLAY_SRC_STREAM, PLAY_SRC_FILE, PLAY_SRC_DIR};

	static uint8_t to_send = 0;
	//int playSrc = PLAY_SRC_STREAM;	//Stream is default

	if (iIndex == 0) {
		for (int i=0; i<iNumParams; i++) {
			if (strcmp(pcParam[i], "token") == 0) {
				//Needs to be implemented
			}
			else if (strncmp(pcParam[i], "url", 4) == 0) {
				printf("play.cgi received URL: %s\r\n", pcValue[i]);
				if (strlen(pcValue[i]) == 0) {
					//Empty url, stop playing
					to_send = VS_MSG_STOP;
					if (xQueueSend(vsQueueHandle, (void*)&to_send, portMAX_DELAY)) {
						printf("VS_MSG_STOP sent to queque\r\n");
					}
					return "/ui/player.shtml";
				}
				else {
					if (strncmp(pcValue[i], "next", 5) == 0) {
						to_send = VS_MSG_NEXT;
						if (xQueueSend(vsQueueHandle, (void*)&to_send, portMAX_DELAY)) {
							printf("VS_MSG_NEXT sent to queque\r\n");
						}
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
		}
	}
	return "/ui/player.shtml";
}

void init_http_server (void) {
	http_set_ssi_handler(ssi_handler, (char const**) TAGS, 1);
	http_set_cgi_handlers(&PLAY_CGI, 1);
}
