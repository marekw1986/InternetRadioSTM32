/*
 * ringbuffer.c
 *
 *  Created on: 27 wrz 2022
 *      Author: marek
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "stm32f1xx_hal.h"
#include "ringbuffer.h"
#include "common.h"
#include "main.h"

static uint8_t buffer[RING_BUFFER_SIZE];
static uint16_t ringbuffer_head = 0;
static uint16_t ringbuffer_tail = 0;

void ringbuffer_clear(void) {
	memset(buffer, 0x00, RING_BUFFER_SIZE);
	ringbuffer_head = 0;
	ringbuffer_tail = 0;
}

void write_array_to_ringbuffer(uint8_t* data, uint16_t len) {

	//printf("Writing to ring buffer, head: %lu, tail: %lu\r\n", spiram_ringbuffer_head, spiram_ringbuffer_tail);

	for (uint16_t i=0; i<len; i++) {
		buffer[ringbuffer_head] = data[i];
		ringbuffer_head++;
		if (ringbuffer_head >= RING_BUFFER_SIZE) {
			ringbuffer_head = 0;
		}
		//If no space in buffer, it will simply override
	}
}

uint16_t read_array_from_ringbuffer(uint8_t* data, uint16_t len) {
	uint32_t bytes_read = 0;

	if (ringbuffer_head == ringbuffer_tail) return 0;

	//printf("Reading from ring buffer, head: %lu, tail: %lu\r\n", spiram_ringbuffer_head, spiram_ringbuffer_tail);

	for(uint16_t i=0; i<len; i++) {
		data[i] = buffer[ringbuffer_tail];
		ringbuffer_tail++;
		bytes_read++;
		if (ringbuffer_tail >= RING_BUFFER_SIZE) {
			ringbuffer_tail = 0;
		}
		if (ringbuffer_tail == ringbuffer_head) { break; }
	}

	return bytes_read;
}

uint16_t get_remaining_space_in_ringbuffer() {
	return (ringbuffer_tail > ringbuffer_head) ? ringbuffer_tail-ringbuffer_head : RING_BUFFER_SIZE - ringbuffer_head + ringbuffer_tail;
}

uint8_t* get_address_of_ringbuffer(void) {
	return buffer;
}
