#ifndef SD_H
#define	SD_H

#define FAIL    0
// Init ERROR code definitions
#define E_COMMAND_ACK     0x80
#define E_INIT_TIMEOUT    0x81

extern unsigned long sdsread, sd_bytesread;
extern char sd_led;

typedef unsigned long LBA;     // logic block address, 32 bit wide


int sd_init( void);     // initializes the SD/MMC memory device
int sd_getCD();              // chech card presence
int sd_getWP();              // check write protection tab
int sd_readSECTOR ( LBA, char *); // reads a block of data
int sd_writeSECTOR( LBA, const char *); // writes a block of data

//unsigned char writeSPI( unsigned char b);


#endif /* SD_H */
