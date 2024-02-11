#include <string.h>
#include <stdio.h>
#include "stream_list.h"
#include "ff.h"

static uint16_t max_stream_id = 0;

static char* find_station_in_file(FIL* file, uint16_t number, char* workbuf, size_t workbuf_len, char* stream_name, size_t stream_name_len);

void initialize_stream_list(void) {
    FIL file;
    FRESULT res;
    uint16_t number = 0;
    char working_buffer[512];
    
    res = f_open(&file, "0:/radio.txt", FA_READ);
    if (res != FR_OK) {
        printf("Can't open file\r\n");
        return;
    }
    while (f_gets(working_buffer, sizeof(working_buffer), &file) != NULL) {
        number++;
    }
    if (number > 0) {
        max_stream_id = number;
    }
    f_close(&file);
}

uint16_t get_max_stream_id(void) {
    return max_stream_id;
}

char* get_station_url_from_file(uint16_t number, char* workbuf, size_t workbuf_len, char* stream_name, size_t stream_name_len) {
    return get_station_url_from_file_use_seek(number, workbuf, workbuf_len, stream_name, stream_name_len, NULL);
}


char* get_station_url_from_file_use_seek(uint16_t number, char* workbuf, size_t workbuf_len, char* stream_name, size_t stream_name_len, int32_t* cur_pos) {
    FIL file;
    FRESULT res;
    char* result = NULL;
    
    res = f_open(&file, "0:/radio.txt", FA_READ);
    if (res != FR_OK) {
        printf("Get station url: Can't open file\r\n");
        return NULL;
    }
    
    if (cur_pos && (*cur_pos > 0)) {
        res = f_lseek(&file, *cur_pos);
        if (res != FR_OK) {
            printf("Get station url: Can't seek to provided value\r\n");
            return NULL;
        }
    }
    
    result = find_station_in_file(&file, number, workbuf, workbuf_len, stream_name, stream_name_len);
    if (cur_pos && (*cur_pos > 0) && (result == NULL)) {
        // It is possible stream is located somewhere earlier in file
        // Look again, just i case if we missed it
        res = f_lseek(&file, 0);
        if (res != FR_OK) {
            printf("Get station url: Can't seek to provided value\r\n");
            return NULL;
        }
        result = find_station_in_file(&file, number, workbuf, workbuf_len, stream_name, stream_name_len);
    }
    
    if (cur_pos) {
        int32_t tmp = f_tell(&file);
        if (tmp > 0) { *cur_pos = tmp; }
    }
    f_close(&file);
    return result;
}

char* find_station_in_file(FIL* file, uint16_t number, char* workbuf, size_t workbuf_len, char* stream_name, size_t stream_name_len) {
	char* result = NULL;
    uint16_t current_id = 0;
    while (f_gets(workbuf, workbuf_len, file) != NULL) {
        current_id++;
        if (current_id == number) {
            if (workbuf[strlen(workbuf)-1] == '\n') {
                workbuf[strlen(workbuf)-1] = '\0';
            }
            if (parse_stream_data_line(workbuf, strlen(workbuf), stream_name, stream_name_len, workbuf, workbuf_len)) {
                result = workbuf;
                break;
            }
            else { break; }
        }
    }
    return result;
}

bool parse_stream_data_line(char* line, size_t line_len, char* stream_name, size_t stream_name_len, char* stream_url, size_t stream_url_len) {
    char* url = strstr(line, " : ");
    if (url) {
        *url = '\0';
        url += 3;
        if (url >= line+line_len) { return 0; }
        if (stream_name) {
            strncpy(stream_name, line, stream_name_len);
        }
        if (stream_url) {
            strncpy(stream_url, url, stream_url_len);
        }
        return true;
    }
    return false;    
}
