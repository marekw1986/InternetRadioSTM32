/*
 * common.c
 *
 *  Created on: 24 sty 2021
 *      Author: marek
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "common.h"


http_res_t parse_http_headers(char* str, size_t len, uri_t* uri) {
	char* tok;

	if (str == NULL) { return HTTP_HEADER_ERROR; }

	if ( (strncmp(str, "HTTP/", 5) != 0) && (strncmp(str, "ICY", 3) != 0) ) {
		return HTTP_HEADER_ERROR;
	}
	tok = strchr(str, ' ');
	if (tok) {
		char* code = tok+1;
		if (code >= str+len) { return HTTP_HEADER_ERROR; }
		if (strncmp(code, "200 OK", 6) == 0) {
			return HTTP_HEADER_OK;
		}
		else if ( (strncmp(code, "301 Moved Permanently", 21) == 0) || (strncmp(code, "302 Moved Temporarily", 21) == 0) || (strncmp(code, "302 Found", 9) == 0) ) {
			char* location = strstr(code, "Location: ");
			if (location) {
				char* newurl = location+10;
				if (newurl >= str+len) { return HTTP_HEADER_ERROR; }
				tok = strstr(newurl, "\r\n");
				if(!tok || (tok >= str+len)) return HTTP_HEADER_ERROR;
				*tok = '\0';
                if (parse_url(newurl, strlen(newurl), uri)) {
                    return HTTP_HEADER_REDIRECTED;
                }
				return HTTP_HEADER_ERROR;
			}
		}
		else {
			printf("Unsupported code\r\n");
			return HTTP_HEADER_ERROR;
		}
	}
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
		if (strncasecmp(url, "https", 5) == 0) {    //HTTPS
			uri->https = 1;
			uri->port = 443;
		}
		else if (strncasecmp(url, "http", 4) == 0) {    //plain old HTTP
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

    if (serverlen > (sizeof(uri->server)-1)) { return 0; }
	memcpy(uri->server, serverbegin, serverlen);
    uri->server[serverlen] = '\0';
	filelen = strlen(rest);
    if (filelen > (sizeof(uri->file)+1)) { return 0; }
	memcpy(uri->file, rest, filelen);
    uri->file[filelen] = '\0';
	return 1;
}
