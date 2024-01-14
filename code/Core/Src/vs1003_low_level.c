/*
 Copyright (C) 2012 Andy Karpov <andy.karpov@gmail.com>
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 Modifications by Marek Wiecek (2023)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tm.h>
#include "vs1003_low_level.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "spiram.h"
#include "common.h"
#include "main.h"

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

extern SPI_HandleTypeDef hspi1;

#define vs1003_chunk_size 32

// VS1003 SCI Write Command byte is 0x02
#define VS_WRITE_COMMAND 0x02

// VS1003 SCI Read COmmand byte is 0x03
#define VS_READ_COMMAND  0x03

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

static void VS1003_print_byte_register(uint8_t reg);
static void VS1003_printDetails(void);
static inline void await_data_request(void);
static inline void control_mode_on(void);
static inline void control_mode_off(void);
static inline void data_mode_on(void);
static inline void data_mode_off(void);
static uint8_t VS1003_SPI_transfer(uint8_t outB);

void VS1003_low_level_init(void) {
  // Keep the chip in reset until we are ready
  HAL_GPIO_WritePin(VS_XRST_GPIO_Port, VS_XRST_Pin, 0);

  // The SCI and SDI will start deselected
  HAL_GPIO_WritePin(VS_XCS_GPIO_Port, VS_XCS_Pin, 1);
  HAL_GPIO_WritePin(VS_XDCS_GPIO_Port, VS_XDCS_Pin, 1);

  // Boot VS1003
  printf("Booting VS1003...\r\n");

  osDelay(1);

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

  osDelay(100);

  /* Switch on the analog parts */
  VS1003_write_register(SCI_VOL,0xfefe); // VOL

  printf("VS1003 still booting\r\n");

  VS1003_write_register(SCI_AUDATA,44101); // 44.1kHz stereo

  VS1003_write_register(SCI_VOL,0x2020); // VOL

  // soft reset
  VS1003_write_register(SCI_MODE, (1 << SM_SDINEW) | (1 << SM_RESET) );
  osDelay(1);
  await_data_request();
  VS1003_write_register(SCI_CLOCKF,0xF800); // Experimenting with highest clock settings
  osDelay(1);
  await_data_request();

  // Now you can set high speed SPI clock
  //SPI configuration
  ////8 bit master mode, CKE=1, CKP=0
  MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_8);       //9 MHz

  printf("VS1003 Set\r\n");
  VS1003_printDetails();
  printf("VS1003 OK\r\n");
}

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

void VS1003_sdi_send_chunk(const uint8_t* data, int len) {
    if (len > 32) return;
    data_mode_on();
    await_data_request();
    while ( len-- ) VS1003_SPI_transfer(*data++);
    data_mode_off();
}

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

feed_ret_t VS1003_feed_from_buffer (void) {
    uint8_t data[32];

    if (!HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin)) return FEED_RET_NO_DATA_NEEDED;
    do {
        if (spiram_get_num_of_bytes_in_ringbuffer() < 32) return FEED_RET_BUFFER_EMPTY;

        uint16_t w = spiram_read_array_from_ringbuffer(data, 32);
        if (w == 32) VS1003_sdi_send_chunk(data, 32);
        asm("nop");
        asm("nop");
        asm("nop");
    } while(HAL_GPIO_ReadPin(VS_DREQ_GPIO_Port, VS_DREQ_Pin));

    return FEED_RET_OK;
}

void VS1003_print_byte_register(uint8_t reg) {
  char extra_tab = strlen(register_names[reg]) < 5 ? '\t' : 0;
  printf("%02x %s\t%c = 0x%02x\r\n", reg, register_names[reg], extra_tab, VS1003_read_register(reg));
}

void VS1003_printDetails(void) {
  printf("VS1003 Configuration:\r\n");
  int i = 0;
  while ( i <= SCI_num_registers )
    VS1003_print_byte_register(i++);
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
	uint8_t answer;

	HAL_SPI_TransmitReceive(&hspi1, &outB, &answer, 1, HAL_MAX_DELAY);
	return answer;
}
