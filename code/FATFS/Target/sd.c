#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "main.h"
#include "sd.h"

unsigned long sd_bytesread,lastsec,init,sdsread=0;

extern SPI_HandleTypeDef hspi3;

volatile int readlock,writelock;
char sd_led;

#ifdef USECACHE
#define CACHESECTORS 2
#define CACHESIZE 512
static unsigned char cache[CACHESIZE];
#endif

// I/O definitions
//#define SDWP    _RG1 // Write Protect input


// SD card commands

#define RESET        0  // a.k.a. GO_IDLE (CMD0)
#define INIT         1  // a.k.a. SEND_OP_COND (CMD1)
/* CMD1: response R1 */
#define CMD_SEND_OP_COND 0x01

/* CMD8: response R7 */
#define CMD_SEND_IF_COND 0x08

#define READ_SINGLE  17
#define WRITE_SINGLE 24
/* CMD55: arg0[31:0]: stuff bits, response R1 */
#define CMD_APP 0x37
/* ACMD41: arg0[31:0]: OCR contents, response R1 */
#define CMD_SD_SEND_OP_COND 0x29
/* CMD58: arg0[31:0]: stuff bits, response R3 */
#define CMD_READ_OCR 0x3a
/* CMD16: arg0[31:0]: block length, response R1 */
#define CMD_SET_BLOCKLEN 0x10


/* command responses */
/* R1: size 1 byte */
#define R1_IDLE_STATE 0
#define R1_ERASE_RESET 1
#define R1_ILL_COMMAND 2
#define R1_COM_CRC_ERR 3
#define R1_ERASE_SEQ_ERR 4
#define R1_ADDR_ERR 5
#define R1_PARAM_ERR 6


/* status bits for card types */
#define SD_RAW_SPEC_1 0
#define SD_RAW_SPEC_2 1
#define SD_RAW_SPEC_SDHC 2


//macros
#define disableSD() HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, 1); clockSPI()
#define enableSD() HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, 0)
#define readSPI() writeSPI( 0xFF)
#define clockSPI() writeSPI( 0xFF)

//#define R_TIMEOUT 25000
#define R_TIMEOUT 255000
#define W_TIMEOUT 250000
//#define I_TIMEOUT 250000
#define I_TIMEOUT 2500

static int sd_raw_card_type;

static void initSD( void) {
    //CS line is high
	HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, 1);

    sd_bytesread=0;
    init=1;

	// This example will use SPI3, initial speed 281 kbps
    if (HAL_SPI_DeInit(&hspi3) != HAL_OK) {
    	printf("SD: SPI3 deinit error\r\n");
    }
	hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
    if (HAL_SPI_Init(&hspi3) != HAL_OK) {
    	printf("SD: SPI3 init error\r\n");
    }
} 

// send one byte of data and receive one back at the same time
unsigned char writeSPI( unsigned char b) {
	unsigned char rcv;
	HAL_SPI_TransmitReceive(&hspi3, &b, &rcv, 1, HAL_MAX_DELAY);
	return rcv;
}// writeSPI


void spi_wait_ready () {
    uint32_t i;

    for (i=0; i<100000; i++){
        if (writeSPI (0xFF) == 0xFF) break;
        //WDT_Reset();
    }
}


int sendSDCmd( unsigned char c, unsigned long a) {
    int i, r;
    // enable SD card
    enableSD();
    if(init) spi_wait_ready();
    // send a     comand packet        (6 bytes)
    writeSPI( c | 0x40);            // send command
    writeSPI( a>>24);              // msb of the address
    writeSPI( a>>16);
    writeSPI( a>>8);
    writeSPI( a);                  // lsb

    switch(c) {
            case RESET:
               writeSPI(0x95);
               break;
            case CMD_SEND_IF_COND:
               writeSPI(0x87);
               break;
            default:
               writeSPI(0xff);
               break;
    }

    /*
    After sending all 6 bytes to the card, we are supposed to wait for a response byte. In fact,
    it is important that we keep sending ?dummy? data continuously clocking the SPI port.
    The response will be 0xFF; basically, the SDI line will be kept high until the card is
    ready to provide a proper response code. The specifications indicate that up to 64 clock
    pulses, or 8 bytes, might be necessary before a proper response is received. Should we
    exceed this limit, we would have to assume a major malfunctioning of the card and abort
    communication: */

    // now wait for a response, allow for up to 8 bytes delay
    for( i=0; i<8; i++) {
       r=readSPI();
       if ( r != 0xFF)
       break;
    }
    return ( r);
    // NOTE CSCD is still low!
} // sendSDCmd


// returns 0 if successful
//          E_COMMAND_ACK   failed to acknowledge reset command
//          E_INIT_TIMEOUT failed to initialize
int sd_init( void) {
    int i, r;

    initSD();

    writelock=readlock=0;
    sd_raw_card_type = 0;
	lastsec=0xffffffff;
    // 1. with the card NOT selected
    disableSD();
    // 2. send 80 clock cycles start up
    for ( i=0; i<10; i++)
        clockSPI();
    // 3. now select the card
    enableSD();
    // 4. send a single RESET command
    r = sendSDCmd( RESET, 0); 
    disableSD();
    if ( r != 1) return E_COMMAND_ACK;               // must return Idle  comand rejected


    /* check for version of SD card specification */
    // r = sendSDCmd(CMD_SEND_IF_COND, 0x100 /* 2.7V - 3.6V */ | 0xaa /* test pattern */);
	r = sendSDCmd(CMD_SEND_IF_COND, 0x1aa);
    if((r & (1 << R1_ILL_COMMAND)) == 0) {
        readSPI();
        readSPI();
        if((readSPI() & 0x01) == 0) return 2; /* card operation voltage range doesn't match */
        if(readSPI() != 0xaa) return 3; /* wrong test pattern */

        /* card conforms to SD 2 card specification */
		printf("SD v2 card\r\n");
        sd_raw_card_type |= (1 << SD_RAW_SPEC_2);
    }
    else {
        /* determine SD/MMC card type */
        sendSDCmd(CMD_APP, 0);
        r = sendSDCmd(CMD_SD_SEND_OP_COND, 0);
        if((r & (1 << R1_ILL_COMMAND)) == 0) {
            /* card conforms to SD 1 card specification */
            sd_raw_card_type |= (1 << SD_RAW_SPEC_1);
			printf("SD v1 card\r\n");
        }
        else {
			printf("MMC card\r\n");
            /* MMC card */
        }
    }

    /* wait for card to get ready */
    for( i = 0; ; ++i) {
        if(sd_raw_card_type & ((1 << SD_RAW_SPEC_1) | (1 << SD_RAW_SPEC_2))) {
            long arg = 0;
            if(sd_raw_card_type & (1 << SD_RAW_SPEC_2)) arg = 0x40000000;
            sendSDCmd(CMD_APP, 0);
            r = sendSDCmd(CMD_SD_SEND_OP_COND, arg);
        }
        else {
            r = sendSDCmd(CMD_SEND_OP_COND, 0);
        }

        if((r & (1 << R1_IDLE_STATE)) == 0) break;

        if(i == 0x7fff) {
            disableSD();
            return 4;
        }
    }

    if(sd_raw_card_type & (1 << SD_RAW_SPEC_2)) {
        //  r = sendSDCmd(CMD_READ_OCR, 0);
        //	sprintf(tbuf,"ocr=%d\r\n",r);uts(tbuf);	
        if(sendSDCmd(CMD_READ_OCR, 0)) {
            disableSD();
            return 5;
        } 

        if(readSPI() & 0x40) sd_raw_card_type |= (1 << SD_RAW_SPEC_SDHC);

        readSPI();
		readSPI();
		readSPI();
    }

    /* set block size to 512 bytes */
    if(sendSDCmd(CMD_SET_BLOCKLEN, 512)) {
        disableSD();
        return 6;
    }

    /* deaddress card */
    disableSD();

/*
  // 5. send repeatedly INIT until Idle terminates
  uts("for...");
  for (i=0; i<I_TIMEOUT; i++){
     r = sendSDCmd( INIT, 0); disableSD();
	//sprintf(tbuf,"%d\r\n",r);uts(tbuf);
     if ( !r) break;
  	}
  if ( i == I_TIMEOUT) return E_INIT_TIMEOUT; // init timed out
  */

/*                                                                    Mass Storage       413
The initialization command can require quite some time, depending on the size and
type of memory card, normally measured in several tenths of a second. Since we are
operating at 250 kb/s, each byte sent will require 32 us. If we consider 6 bytes for every
command retry, using a count up to 10,000 will provide us with a generous timeout limit
(I_TIMEOUT) of approximately three tenths of a second as per SD card specifications.
It is only upon successful completion of the preceding sequence that we will be allowed
to finally switch gear and dramatically increase the clock speed to the highest possible
value supported by our hardware. With minimal experimentation you will find that an
Explorer 16 board, with a properly designed daughter board providing the SD/MMC
connector, can easily sustain a clock rate as high as 18 MHz. This value can be obtained
by reconfiguring the SPI baud rate generator for a 1:2 ratio. We can now complete the
initMedia() function with the last segment: */

    // 6. increase speed
    if (HAL_SPI_DeInit(&hspi3) != HAL_OK) {
    	printf("SD: SPI3 deinit error\r\n");
    }
	hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    if (HAL_SPI_Init(&hspi3) != HAL_OK) {
    	printf("SD: SPI3 init error\r\n");
    }
	init=0;
    return 0;
} // init media

#define DATA_START 0xFE


// a        LBA of sector requested
// p        pointer to sector buffer
// returns TRUE if successful
int sd_readSECTOR( LBA a, char *p) {
    int r, i;  //  int r, i,j;

    sd_led=1;
    #ifdef SD_LOCK
    while(readlock);
    readlock=1;
    #endif
    // 1. send READ command
    // r = sendSDCmd( READ_SINGLE, ( a << 9));

    #ifdef USECACHE
    if(a==lastsec){
        //printf((ROM char*)"SD: read sector 0x%X from cache\r\n",a);
        for(r=0;r<512;r++) *p++ = cache[r]; 
        sd_bytesread=sd_bytesread+512; 
        return 1;	
    }

    /*
    for (i=0;i<CACHESECTORS;i++){
        if(a==sectorincache[i]) {
            printf((ROM char*)"SD: read sector 0x%X from cache\r\n",a);
            for(r=0;r<512;r++) *p++ = cache[i][r];	 
            sd_bytesread=sd_bytesread+512;				
        }
    }*/

    #endif

    lastsec=a;

    if(sd_raw_card_type & (1 << SD_RAW_SPEC_SDHC)) r = sendSDCmd( READ_SINGLE, ( a << 9)/512); 
    else r = sendSDCmd( READ_SINGLE, ( a << 9));

    if ( r == 0) {   // check if command was accepted
        // 2. wait for a response
        for( i=0; i<R_TIMEOUT; i++) {
            r = readSPI();
            if ( r == DATA_START)
            break;
        }
        // 3. if it did not timeout, read 512 byte of data
        if ( i != R_TIMEOUT) {
            i = 0;
		 
            do {
                sdsread++;
                #ifdef USECACHE
                 cache[i]= 
                #endif
                *p++ = readSPI();
                i++;
            } while (i<512);
            sd_bytesread=sd_bytesread+512;
            // 4. ignore CRC
            readSPI();
            readSPI();
        } // data arrived
		else {
			printf("readSECTOR %ld R_TIMEOUT return -2\r\n",a);
			readlock=0;
            return 0;
        }
    } // command accepted
	else {
		printf("readSECTOR  %ld cmd rejected return -1\r\n",a);
		readlock=0;	
        return 0;
	}
    // 5. remember to disable the card
    disableSD();
    #ifdef SD_LOCK
    readlock=0;
    #endif
    return ( r == DATA_START);    // return TRUE if successful
} // readSECTOR


#define DATA_ACCEPT                 0x05
// a       LBA of sector requested
// p       pointer to sector buffer
// returns TRUE if successful
int sd_writeSECTOR( LBA a, const char *p) {
    unsigned r, i;
    sd_led=1;
    #ifdef SD_LOCK
    while(writelock);
    writelock=1;
    #endif

    // 1. send WRITE command
    //  r = sendSDCmd( WRITE_SINGLE, ( a << 9));
    if(sd_raw_card_type & (1 << SD_RAW_SPEC_SDHC)) r = sendSDCmd( WRITE_SINGLE, ( a << 9)/512);
    else r = sendSDCmd( WRITE_SINGLE, ( a << 9));

    if ( r == 0) {     // check if command was accepted
        // 2. send data
        writeSPI( DATA_START);
        // send 512 bytes of data
        for( i=0; i<512; i++) writeSPI( *p++);
        // 3. send dummy CRC
        clockSPI();
        clockSPI();
        // 4. check if data accepted
        r = readSPI();
        if ( (r & 0xf) == DATA_ACCEPT) {
            // 5. wait for write completion
            for( i=0; i<W_TIMEOUT; i++) {
                r = readSPI();
                if ( r != 0 )
                break;
            }
        } // accepted
        else {
            r = FAIL;
            printf("writeSECTOR %ld failed r=%d\n",a,r);
		}
    } // command accepted
	else {
		printf("writeSECTOR %ld cmd rejected r=%d\n",a,r);
	}
    // 6. remember to disable the card

    disableSD();
    #ifdef SD_LOCK
      writelock=0;
    #endif
    return ( r);     // return TRUE if successful
} // writeSECTOR

// SD card connector presence detection switch
int sd_getCD( void)
// returns TRUE card present
//          FALSE card not present
{
  return !HAL_GPIO_ReadPin(SD_PRESENT_GPIO_Port, SD_PRESENT_Pin);
}




