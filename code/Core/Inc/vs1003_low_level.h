#ifndef _VS1003_LOW_LEVEL_H_
#define _VS1003_LOW_LEVEL_H_

#include <stdint.h>

// SCI Registers

#define SCI_MODE            0x0
#define SCI_STATUS          0x1
#define SCI_BASS            0x2
#define SCI_CLOCKF          0x3
#define SCI_DECODE_TIME     0x4
#define SCI_AUDATA          0x5
#define SCI_WRAM            0x6
#define SCI_WRAMADDR        0x7
#define SCI_HDAT0           0x8
#define SCI_HDAT1           0x9
#define SCI_AIADDR          0xa
#define SCI_VOL             0xb
#define SCI_AICTRL0         0xc
#define SCI_AICTRL1         0xd
#define SCI_AICTRL2         0xe
#define SCI_AICTRL3         0xf
#define SCI_num_registers   0xf

// SCI_MODE bits

#define SM_DIFF             0
#define SM_LAYER12          1
#define SM_RESET            2
#define SM_OUTOFWAV         3
#define SM_EARSPEAKER_LO    4
#define SM_TESTS            5
#define SM_STREAM           6
#define SM_EARSPEAKER_HI    7
#define SM_DACT             8
#define SM_SDIORD           9
#define SM_SDISHARE         10
#define SM_SDINEW           11
#define SM_ADPCM            12
#define SM_ADCPM_HP         13
#define SM_LINE_IN          14

typedef enum {
    FEED_RET_NO_DATA_NEEDED = 0,
    FEED_RET_OK,
    FEED_RET_BUFFER_EMPTY
} feed_ret_t;

void VS1003_low_level_init(void);
uint16_t VS1003_read_register(uint8_t _reg);
void VS1003_write_register(uint8_t _reg,uint16_t _value);
void VS1003_sdi_send_buffer(const uint8_t* data, int len);
void VS1003_sdi_send_chunk(const uint8_t* data, int len);
void VS1003_sdi_send_zeroes(int len);
feed_ret_t VS1003_feed_from_buffer (void);

#endif // _VS1003_LOW_LEVEL_H_
