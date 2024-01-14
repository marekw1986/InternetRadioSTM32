/* 
 * File:   vs1003.h
 * Author: Przemyslaw Stasiak
 *
 * Created on 4 maja 2020, 10:33
 */

#ifndef VS1003_H
#define	VS1003_H

#include <stdlib.h>
#include <stdint.h>

#define VS1003_MEASURE_STREAM_BITRATE

#define RXNE    0x01
#define TXE     0x02
#define BSY     0x80

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

enum {VS_MSG_NEXT, VS_MSG_STOP, VS_MSG_PLAY_BY_ID, VS_MSG_SET_VOL};

typedef struct {
	uint8_t cmd;
	uint16_t param;
	uint8_t reserved;
} vs1003cmd_t;

#ifdef	__cplusplus
extern "C" {
#endif

//public functions
void VS1003_init(void);
void VS1003_handle (void);
void VS1003_setVolume(uint8_t vol);
void VS1003_loadUserCode(const uint16_t* buf, size_t len);
void VS1003_play_next(void);
void VS1003_play_prev(void);
void VS1003_play_next_audio_file_from_directory (void);
void VS1003_play_http_stream(const char* url);
void VS1003_play_http_stream_by_id(uint16_t id);
void VS1003_play_next_http_stream_from_list(void);
void VS1003_play_prev_http_stream_from_list(void);
void VS1003_play_file (char* url);
void VS1003_play_dir (const char* url);
void VS1003_stop(void);
void VS1003_setLoop(uint8_t val);
uint8_t VS1003_getLoop(void);
uint16_t VS1003_get_remaining_space_in_ringbuffer(void);
void VS1003_send_cmd_thread_safe(uint8_t cmd, uint16_t param);


#ifdef	__cplusplus
}
#endif

#endif	/* VS1003_H */

