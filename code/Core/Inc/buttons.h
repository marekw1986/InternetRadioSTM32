/*
 * buttons.h
 *
 *  Created on: 9 paź 2022
 *      Author: marek
 */

#ifndef INC_BUTTONS_H_
#define INC_BUTTONS_H_

#include <stdio.h>
#include <stdint.h>
#include "main.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct button {
	GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t timer;
    uint8_t state;
    void (*push_proc)(void);
    void (*long_proc)(void);
} button_t;

void button_init(button_t *btn, GPIO_TypeDef *port, uint16_t pin, void (*push_proc)(void), void (*long_proc)(void));
void button_handle(button_t *btn);

//void rotary_irq (void);
//int16_t rotary_handle (void);


#endif /* INC_BUTTONS_H_ */
