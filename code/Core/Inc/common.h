/*
 * common.h
 *
 *  Created on: 24 sty 2021
 *      Author: marek
 */

#ifndef INC_COMMON_H_
#define INC_COMMON_H_

#define millis() HAL_GetTick()

#define URI_SERVER_SIZE 256
#define URI_FILE_SIZE 256

typedef struct {
	char server[URI_SERVER_SIZE+1];
	char file[URI_FILE_SIZE+1];
	uint8_t https;
	uint16_t port;
} uri_t;

typedef enum {HTTP_HEADER_ERROR = 0, HTTP_HEADER_IN_PROGRESS, HTTP_HEADER_OK, HTTP_HEADER_REDIRECTED} http_res_t;

void prepare_http_parser(void);
http_res_t parse_http_headers(char* str, size_t len, uri_t* uri);
uint8_t parse_url (const char* str, size_t len, uri_t* uri);

#endif /* INC_COMMON_H_ */
