#include <stdlib.h>
#include <stdint.h>
#include "FreeRTOS.h"

#include "http_header_parser.h"

#define WORKING_BUFFER_SIZE 512
static char* working_buffer = NULL;
static uint16_t http_code = 0;
static http_err_t error_code = HTTP_ERR_UNKNOWN;

static void analyze_line(char* line, uint16_t len, uri_t* uri);
static http_res_t http_finalize_parsing(uri_t* uri);

bool http_prepare_parser(void) {
    if (working_buffer) { return false; }
    http_code = 0;
    error_code = HTTP_ERR_UNKNOWN;
    working_buffer = (char*)pvPortMalloc(WORKING_BUFFER_SIZE*sizeof(char));
    if (working_buffer == NULL) return false;
    memset(working_buffer, 0x00, WORKING_BUFFER_SIZE);
    return true;
}

void http_release_parser(void) {
    if (working_buffer) {
        vPortFree(working_buffer);
        working_buffer = NULL;
        http_code = 0;
        error_code = HTTP_ERR_UNKNOWN;
    }
}

http_err_t http_get_err_code(void) {
    return error_code;
}

static http_res_t http_finalize_parsing(uri_t* uri) {
    switch(http_code) {
        case 200:
        return HTTP_HEADER_OK;
        error_code = HTTP_NO_ERR;
        break;
        
        case 301:
        case 302:
        if ( (strlen(uri->server) == 0) || (strlen(uri->file) == 0) || (uri->port == 0) ) {
            error_code = HTTP_ERR_NO_DATA;
            return HTTP_HEADER_ERROR;
        }
        return HTTP_HEADER_REDIRECTED;
        error_code = HTTP_NO_ERR;
        break;
        
        default:
        return HTTP_HEADER_ERROR;
        error_code = HTTP_ERR_UNKNOWN;
        break;
    }
    
    return HTTP_HEADER_ERROR;
}

static void analyze_line(char* line, uint16_t len, uri_t* uri) {
    char* tok;
    tok = strstr(line, ": ");
    if (tok == NULL) { return; }    //Delimiter not found
    tok[0] = '\0';
    tok[1] = '\0';
    tok += 2;
    if (strncmp(line, "Location", strlen(line)) == 0) {
        parse_url(tok, strlen(tok), uri);
    }
}

http_res_t http_parse_headers(char* str, size_t len, uri_t* uri) {
	char* tok;
    char* next_line;
	
    if (working_buffer == NULL) {
        error_code = HTTP_ERR_ALOC;
        return HTTP_HEADER_ERROR;
    }
	if (str == NULL) {
        error_code = HTTP_ERR_STR_NULL;
        return HTTP_HEADER_ERROR;
    }
    
    strncat(working_buffer, str, len);
    
    tok = strstr(working_buffer, "\r\n");
    if (!tok) {
        //There is no complete line. Return to continue later.
        return HTTP_HEADER_IN_PROGRESS;
    }
    if (tok > working_buffer+WORKING_BUFFER_SIZE) {
        error_code = HTTP_ERR_BUF_OVERFLOW;
        return HTTP_HEADER_ERROR;
    }
    tok[0] = '\0';
    tok[1] = '\0';
    next_line = tok + 2;
    //printf("Current line: %s\r\n", working_buffer);
    
    if (strlen(working_buffer) == 0) {
        return http_finalize_parsing(uri);
    }
	
	if (http_code == 0) {
        //First line is not parsed
        if ( (strncmp(working_buffer, "HTTP/", 5) == 0) || (strncmp(str, "ICY", 3) == 0) ) {
            tok = strchr(working_buffer, ' ');
            if (tok == NULL) {
                error_code = HTTP_ERR_PARSING_CODE;
                return HTTP_HEADER_ERROR;
            }
            tok++;
            http_code = atoi(tok);
            if (http_code == 0) {
                error_code = HTTP_ERR_PARSING_CODE;
                return HTTP_HEADER_ERROR;
            }
            //Now we begin parsing parameters, so clean uri
            memset(uri, 0x00, sizeof(uri_t));
        }
    }
    else {
        //First line already parsed, parsing next lines
        analyze_line(working_buffer, WORKING_BUFFER_SIZE, uri);
    }
    
    strncpy(working_buffer, next_line, WORKING_BUFFER_SIZE);    
    if (memcmp(working_buffer, "\r\n", 2) == 0) {
        return http_finalize_parsing(uri);
    }
    
    return HTTP_HEADER_IN_PROGRESS;
}

uint8_t parse_url (const char* url, size_t len, uri_t* uri) {
	const char* tok;
    const char* serverbegin = url;
	const char* rest;
	const char* port;
	size_t serverlen = 0;
    size_t filelen = 0;
    
	if (uri == NULL) return 0;
	
	tok = strstr(url, "://");
	if (tok) {
		if (tok >= url+len) return 0;
		if (strncmp(url, "https", 5) == 0) {    //HTTPS
			uri->https = 1;
			uri->port = 443;
		}
		else if (strncmp(url, "http", 4) == 0) {    //plain old HTTP
			uri->https = 0;
			uri->port = 80;
		}
		else {
			//It is different protocol
			return 0;
		}
		rest = tok+3;
        serverbegin = rest;
		if (rest >= url+len) return 0;
	}
	else {
		//printf("No protocol specified - assuming HTTP\r\n");
		uri->https = 0;
		uri->port = 80;
	}
	
	//printf("%s\r\n", url);
	
	tok = strchr(serverbegin, ':');
	if (tok) {
		if (tok >= url+len) return 0;
        serverlen = tok-serverbegin;
		rest = tok+1;
		if (rest >= url+len) return 0;
		//printf("Wykryto dwukropek: %s\r\n", rest);
		tok = strchr(rest, '/');
		if (tok) {
			if (tok >= url+len) return 0;
			//printf("Wykryto znak /, wszystko ok. %s\r\n", tok);
			port = rest;
            size_t portlen = tok-port;
            if (portlen > 5) {
                return 0;
            }
            char tmp[6];
            memcpy((char*)tmp, (const char*)port, portlen);
            tmp[portlen] = '\0';
			uri->port = atoi(tmp);
			if (uri->port == 0) {
				//printf("Nieprawid?owy port %d\r\n", uri->port);
				return 0;
			}
			rest = tok;
		}
		else {
			//printf("Oczekiwano znaku /, cos jest nie tak\r\n");
			return 0;
		}
	}
	else {
		//printf("Nie wykryto dwukropka\r\n");
		tok = strchr(serverbegin, '/');
		if (tok) {
			if (tok >= url+len) { return 0; }
            serverlen = tok-serverbegin;
			rest = tok;
			if (rest >= url+len) { return 0; }
		}
		else {
			//printf("Oczekiwano znaku /, cos jest nie tak\r\n");
			return 0;
		}
	}
    
    if (serverlen > (sizeof(uri->server))) { return 0; }
	memcpy(uri->server, serverbegin, serverlen);
    uri->server[serverlen] = '\0';
	filelen = strlen(rest);
    if (filelen > (sizeof(uri->file))) { return 0; }
	memcpy(uri->file, rest, filelen);
    uri->file[filelen] = '\0';
	return 1;
}
