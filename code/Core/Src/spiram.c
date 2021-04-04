/*
 * spiram.c
 *
 *  Created on: 4 kwi 2021
 *      Author: marek
 */

#include <stdlib.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "main.h"
#include "spiram.h"

#define RDMR        5       // Read the Mode Register
#define WRMR        1       // Write to the Mode Register
#define READ        3       // Read command
#define WRITE       2       // Write command
#define RSTIO     	0xFF    // Reset memory to SPI mode
#define BYTE_MODE   0x00    // Byte mode (read/write one byte at a time)
#define SEQ_MODE 	0x40    // Sequential mode (read/write blocks of memory)

extern SPI_HandleTypeDef hspi3;

static void spiram_setmode(uint8_t mode ) {
	uint8_t tmp;

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 0);
	tmp = WRMR;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi3, &mode, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 1);
}


void spiram_clear(void) {
	uint8_t tmp;
	int i;

	spiram_setmode(SEQ_MODE);
	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 0);

	tmp = WRITE;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	tmp = 0x00;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	for (i=0; i<0x20000; i++) {
		HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	}

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 1);

}


void spiram_writebyte(uint32_t address, uint8_t data) {
	uint8_t tmp;

	spiram_setmode(BYTE_MODE);
	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 0);

	tmp = WRITE;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	tmp = (uint8_t)(address >> 16);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)(address >> 8);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)address;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	HAL_SPI_Transmit(&hspi3, &data, 1, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 1);
}


uint8_t spiram_readbyte(uint32_t address) {
	uint8_t tmp;

	spiram_setmode(BYTE_MODE);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 0);

	tmp = READ;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	tmp = (uint8_t)(address >> 16);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)(address >> 8);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)address;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	HAL_SPI_Receive(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 1);
	return tmp;
}


void spiram_writearray(uint32_t address, uint8_t *data, uint16_t len) {
	uint8_t tmp;

	spiram_setmode(SEQ_MODE);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 0);

	tmp = WRITE;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	tmp = (uint8_t)(address >> 16);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)(address >> 8);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)address;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	HAL_SPI_Receive(&hspi3, data, len, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 1);
}


void spiram_readarray(uint32_t address, uint8_t *data, uint16_t len) {
	uint8_t tmp;

	spiram_setmode(SEQ_MODE);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 0);

	tmp = READ;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	tmp = (uint8_t)(address >> 16);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)(address >> 8);
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);
	tmp = (uint8_t)address;
	HAL_SPI_Transmit(&hspi3, &tmp, 1, HAL_MAX_DELAY);

	HAL_SPI_Receive(&hspi3, data, len, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(SPIRAM_CS_GPIO_Port, SPIRAM_CS_Pin, 1);
}
