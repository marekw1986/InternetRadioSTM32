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

#define RXNE    0x01
#define TXE     0x02
#define BSY     0x80

#ifdef	__cplusplus
extern "C" {
#endif

//public functions
void VS1003_begin(void);    
uint16_t VS1003_read_register(uint8_t _reg);
void VS1003_write_register(uint8_t _reg,uint16_t _value);
void VS1003_sdi_send_buffer(const uint8_t* data, int len);
void VS1003_sdi_send_zeroes(int len);
uint8_t VS1003_feed_from_buffer (void);
void VS1003_handle (void);
void VS1003_setVolume(uint8_t vol);
void VS1003_playChunk(const uint8_t* data, size_t len);
void VS1003_print_byte_register(uint8_t reg);
void VS1003_printDetails(void);
void VS1003_loadUserCode(const uint16_t* buf, size_t len);
void VS1003_play_next_audio_file_from_directory (void);
void VS1003_play_http_stream(const char* url);
void VS1003_play_next_http_stream_from_list(void);
void VS1003_play_file (char* url);
void VS1003_play_dir (const char* url);
void VS1003_stop(void);
void VS1003_setLoop(uint8_t val);
uint8_t VS1003_getLoop(void);


#ifdef	__cplusplus
}
#endif

#endif	/* VS1003_H */

