/*
 * spiram.h
 *
 *  Created on: 4 kwi 2021
 *      Author: marek
 */

#ifndef INC_SPIRAM_H_
#define INC_SPIRAM_H_

#include <stdint.h>

void spiram_clear(void);
void spiram_writebyte(uint32_t address, uint8_t data);
uint8_t spiram_readbyte(uint32_t address);
void spiram_writearray(uint32_t address, uint8_t *data, uint16_t len);
void spiram_readarray(uint32_t address, uint8_t *data, uint16_t len);
void spiram_write_array_to_ringbuffer(uint8_t* data, uint16_t len);
uint32_t spiram_read_array_from_ringbuffer(uint8_t* data, uint32_t len);
uint32_t spiram_get_remaining_space_in_ringbuffer();


#endif /* INC_SPIRAM_H_ */
