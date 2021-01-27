/*
 Copyright (C) 2012 Andy Karpov <andy.karpov@gmail.com>
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "vs1003.h"
#include "time.h"
#include "stm32f1xx_hal.h"
#include "main.h"

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

extern SPI_HandleTypeDef hspi1;

#define vs1003_chunk_size 32

/****************************************************************************/

// VS1003 SCI Write Command byte is 0x02
#define VS_WRITE_COMMAND 0x02

// VS1003 SCI Read COmmand byte is 0x03
#define VS_READ_COMMAND  0x03

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

// Register names

typedef enum {
    reg_name_MODE = 0,
    reg_name_STATUS,
    reg_name_BASS,
    reg_name_CLOCKF,
    reg_name_DECODE_TIME,
    reg_name_AUDATA,
    reg_name_WRAM,
    reg_name_WRAMADDR,
    reg_name_HDAT0,
    reg_name_HDAT1,
    reg_name_AIADDR,
    reg_name_VOL,
    reg_name_AICTRL0,
    reg_name_AICTRL1,
    reg_name_AICTRL2,
    reg_name_AICTRL3
} register_names_t;

const char * register_names[] =
{
  "MODE",
  "STATUS",
  "BASS",
  "CLOCKF",
  "DECODE_TIME",
  "AUDATA",
  "WRAM",
  "WRAMADDR",
  "HDAT0",
  "HDAT1",
  "AIADDR",
  "VOL",
  "AICTRL0",
  "AICTRL1",
  "AICTRL2",
  "AICTRL3",
};

 static inline void await_data_request(void);
 static inline void control_mode_on(void);
 static inline void control_mode_off(void);
 static inline void data_mode_on(void);
 static inline void data_mode_off(void);
 static uint8_t VS1003_SPI_transfer(uint8_t outB);


/****************************************************************************/

uint16_t VS1003_read_register(uint8_t _reg) {
  uint16_t result;
  control_mode_on();
  delay_us(1); // tXCSS
  VS1003_SPI_transfer(VS_READ_COMMAND); // Read operation
  VS1003_SPI_transfer(_reg); // Which register
  result = VS1003_SPI_transfer(0xff) << 8; // read high byte
  result |= VS1003_SPI_transfer(0xff); // read low byte
  delay_us(1); // tXCSH
  await_data_request();
  control_mode_off();
  return result;
}

/****************************************************************************/

void VS1003_write_register(uint8_t _reg,uint16_t _value) {
  control_mode_on();
  delay_us(1); // tXCSS
  VS1003_SPI_transfer(VS_WRITE_COMMAND); // Write operation
  VS1003_SPI_transfer(_reg); // Which register
  VS1003_SPI_transfer(_value >> 8); // Send hi byte
  VS1003_SPI_transfer(_value & 0xff); // Send lo byte
  delay_us(1); // tXCSH
  await_data_request();
  control_mode_off();
}

/****************************************************************************/

void VS1003_sdi_send_buffer(const uint8_t* data, int len) {
  int chunk_length;
  
  data_mode_on();
  while ( len ) {
    await_data_request();
    delay_us(3);
    chunk_length = min(len, vs1003_chunk_size);
    len -= chunk_length;
    while ( chunk_length-- ) VS1003_SPI_transfer(*data++);
  }
  data_mode_off();
}

/****************************************************************************/

void VS1003_sdi_send_zeroes(int len) {
  int chunk_length;  
  data_mode_on();
  while ( len ) {
    await_data_request();
    chunk_length = min(len,vs1003_chunk_size);
    len -= chunk_length;
    while ( chunk_length-- ) VS1003_SPI_transfer(0);
  }
  data_mode_off();
}

/****************************************************************************/

void VS1003_begin(void) {
  // Keep the chip in reset until we are ready
  HAL_GPIO_WritePin(VS_XRST_GPIO_Port, VS_XRST_Pin, 0);

  // The SCI and SDI will start deselected
  HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
  HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);

  // Boot VS1003
  printf("Booting VS1003...\r\n");

  HAL_Delay(1);

  // init SPI slow mode
  //SPI configuration
  //8 bit master mode, CKE=1, CKP=0
  MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_256); //281 kHz

  // release from reset
  HAL_GPIO_WritePin(VS_XRST_GPIO_Port, VS_XRST_Pin, 1);
  
  // Declick: Immediately switch analog off
  VS1003_write_register(SCI_VOL,0xffff); // VOL

  /* Declick: Slow sample rate for slow analog part startup */
  VS1003_write_register(SCI_AUDATA,10);

  HAL_Delay(100);

  /* Switch on the analog parts */
  VS1003_write_register(SCI_VOL,0xfefe); // VOL
  
  printf("VS1003 still booting\r\n");

  VS1003_write_register(SCI_AUDATA,44101); // 44.1kHz stereo

  VS1003_write_register(SCI_VOL,0x2020); // VOL
  
  // soft reset
  VS1003_write_register(SCI_MODE, (1 << SM_SDINEW) | (1 << SM_RESET) );
  HAL_Delay(1);
  await_data_request();
  VS1003_write_register(SCI_CLOCKF,0xB800); // Experimenting with higher clock settings
  HAL_Delay(1);
  await_data_request();

  // Now you can set high speed SPI clock
  //SPI configuration
  ////8 bit master mode, CKE=1, CKP=0
  MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_32);       //2.5 MHz

  printf("VS1003 Set\r\n");
  VS1003_printDetails();
  printf("VS1003 OK\r\n");
}

/****************************************************************************/

void VS1003_setVolume(uint8_t vol) {
  uint16_t value = vol;
  value <<= 8;
  value |= vol;

  VS1003_write_register(SCI_VOL,value); // VOL
}

/****************************************************************************/

void VS1003_startSong(void) {
  VS1003_sdi_send_zeroes(10);
}

/****************************************************************************/

void VS1003_playChunk(const uint8_t* data, size_t len) {
  VS1003_sdi_send_buffer(data,len);
}

/****************************************************************************/

void VS1003_stopSong(void) {
  VS1003_sdi_send_zeroes(2048);
}

/****************************************************************************/

void VS1003_print_byte_register(uint8_t reg) {
  char extra_tab = strlen(register_names[reg]) < 5 ? '\t' : 0;
  printf("%02x %s\t%c = 0x%02x\r\n", reg, register_names[reg], extra_tab, VS1003_read_register(reg));
}

/****************************************************************************/

void VS1003_printDetails(void) {
  printf("VS1003 Configuration:\r\n");
  int i = 0;
  while ( i <= SCI_num_registers )
    VS1003_print_byte_register(i++);
}

/****************************************************************************/

void VS1003_loadUserCode(const uint16_t* buf, size_t len) {
  while (len) {
    uint16_t addr = *buf++; len--;
    uint16_t n = *buf++; len--;
    if (n & 0x8000U) { /* RLE run, replicate n samples */
      n &= 0x7FFF;
      uint16_t val = *buf++; len--;
      while (n--) {
	    printf("W %02x: %04x\r\n", addr, val);
        VS1003_write_register(addr, val);
      }
    } else {           /* Copy run, copy n samples */
      while (n--) {
	uint16_t val = *buf++; len--;
	printf("W %02x: %04x\r\n", addr, val);
        VS1003_write_register(addr, val);
      }
    }
  }
}


  static inline void await_data_request(void) {
    while ( !HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin) );
  }

  static inline void control_mode_on(void) {
    HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);
    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 0);
  }

  static inline void control_mode_off(void) {
	  HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
  }

  static inline void data_mode_on(void) {
    HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
    HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 0);
  }

  static inline void data_mode_off(void) {
	  HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);
  }
  
  static uint8_t VS1003_SPI_transfer(uint8_t outB) {
	uint8_t data;

	HAL_SPI_TransmitReceive(&hspi1, &outB, &data, 1, 100);
    return data;
}
  

/*
void VS1003_SPI_conf(){
	//SPI1 configuration     
    SPI1CON = (_SPI1CON_ON_MASK  | _SPI1CON_CKE_MASK | _SPI1CON_MSTEN_MASK);    //8 bit master mode, CKE=1, CKP=0
    SPI1BRG = (GetPeripheralClock()-1ul)/2ul/4000000;       //4MHz
}

uint8_t VS1003_SPIPutChar(uint8_t outB){
    SPI1BUF = outB;
    while (SPI1STATbits.SPITBF);
    while (!SPI1STATbits.SPIRBF);
    return SPI1BUF;
}
*/
