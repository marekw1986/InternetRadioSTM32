/* 
 * File:   mediainfo.h
 * Author: marek
 *
 * Created on 7 marca 2023, 20:20
 */

#ifndef MEDIAINFO_H
#define	MEDIAINFO_H

#include <stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif
    
typedef enum {MEDIA_TYPE_FILE = 0, MEDIA_TYPE_STREAM} mediainfo_type_t;

#define GENRES_NUM 148
extern const char *genres[GENRES_NUM];

void mediainfo_clean (void);
char* mediainfo_title_get(void);
void mediainfo_title_set(const char* src);
char* mediainfo_artist_get(void);
void mediainfo_artist_set(const char* src);
char* mediainfo_album_get(void);
void mediainfo_album_set(const char* src);
char* mediainfo_year_get(void);
void mediainfo_year_set(const char* src);
char* mediainfo_comment_get(void);
void mediainfo_comment_set(const char* src);
const char* mediainfo_genre_get(void);
void mediainfo_genre_set(uint16_t src);
mediainfo_type_t mediainfo_type_get(void);
void mediainfo_type_set(mediainfo_type_t src);



#ifdef	__cplusplus
}
#endif

#endif	/* MEDIAINFO_H */

