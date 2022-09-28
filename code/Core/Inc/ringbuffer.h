/*
 * ringbuffer.h
 *
 *  Created on: 27 wrz 2022
 *      Author: marek
 */
#include <stdint.h>

#ifndef INC_RINGBUFFER_H_
#define INC_RINGBUFFER_H_

#define RING_BUFFER_SIZE 8192

void ringbuffer_clear(void);
void write_array_to_ringbuffer(uint8_t* data, uint16_t len);
uint16_t read_array_from_ringbuffer(uint8_t* data, uint16_t len);
uint8_t* get_address_of_ringbuffer(void);
uint16_t get_remaining_space_in_ringbuffer();

#endif /* INC_RINGBUFFER_H_ */
