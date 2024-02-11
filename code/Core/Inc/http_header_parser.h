#ifndef _HTTP_HEADER_PARSER_H_
#define _HTTP_HEADER_PARSER_H_

#include <stdint.h>
#include <string.h>
#include "common.h"

#define URI_SERVER_SIZE 256
#define URI_FILE_SIZE 256

typedef struct {
	char server[URI_SERVER_SIZE+1];
	char file[URI_FILE_SIZE+1];
	uint8_t https;
	uint16_t port;
} uri_t;

typedef enum {HTTP_HEADER_ERROR = 0, HTTP_HEADER_IN_PROGRESS, HTTP_HEADER_OK, HTTP_HEADER_REDIRECTED} http_res_t;
typedef enum {HTTP_ERR_UNKNOWN = 0, HTTP_NO_ERR, HTTP_ERR_ALOC, HTTP_ERR_STR_NULL, HTTP_ERR_BUF_OVERFLOW, HTTP_ERR_NO_DATA, HTTP_ERR_PARSING_CODE} http_err_t;

#ifdef	__cplusplus
extern "C" {
#endif
bool http_prepare_parser(void);
void http_release_parser(void);
http_err_t http_get_err_code(void);
http_res_t http_parse_headers(char* str, size_t len, uri_t* uri);
uint8_t parse_url (const char* str, size_t len, uri_t* uri);
#ifdef	__cplusplus
}
#endif

#endif // _HTTP_HEADER_PARSER_H_