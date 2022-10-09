/*
 * buttons.c
 *
 *  Created on: 9 paź 2022
 *      Author: marek
 */


#include <stdint.h>
#include "buttons.h"

//#define ROTARY_CLK_PIN 	HAL_GPIO_ReadPin(SW_A_GPIO_Port, SW_A_Pin)
//#define ROTARY_DT_PIN 	HAL_GPIO_ReadPin(SW_B_GPIO_Port, SW_B_Pin)
//#define KEYS_LOCKED		HAL_GPIO_ReadPin(LOCK_SWITCH_GPIO_Port, LOCK_SWITCH_Pin)

//volatile uint8_t rotary_flag = 0;
//volatile int16_t rotary_counter = 0;


void button_init(button_t *btn, GPIO_TypeDef *port, uint16_t pin, void (*push_proc)(void), void (*long_proc)(void)) {
    btn->port = port;
    btn->pin = pin;
    btn->push_proc = push_proc;
    btn->long_proc = long_proc;
    btn->timer = 0;
    btn->state = 0;
}


void button_handle(button_t *btn) {
    uint8_t pressed;
    enum {IDLE, DEBOUNCE, WAIT_LONG, WAIT_RELEASE};

    pressed = !HAL_GPIO_ReadPin(btn->port, btn->pin);
    if(pressed && !(btn->state)) {
        btn->state = DEBOUNCE;
        btn->timer = HAL_GetTick();
    }
    else if (btn->state) {
        if(pressed && (btn->state)==DEBOUNCE && ((uint32_t)(HAL_GetTick()-(btn->timer)) >= 20)) {
            btn->state = WAIT_LONG;
            btn->timer = HAL_GetTick();
        }
        else if (!pressed && (btn->state)==WAIT_LONG) {
            if (btn->push_proc) (btn->push_proc)();
            btn->state = IDLE;
        }
        else if (pressed && (btn->state)==WAIT_LONG && ((uint32_t)(HAL_GetTick()-(btn->timer)) >= 2000)) {
            if (btn->long_proc) (btn->long_proc)();
            btn->state = WAIT_RELEASE;
            btn->timer = 0;
        }
    }
    if ((btn->state)==WAIT_RELEASE && !pressed) btn->state = IDLE;
}


//int16_t rotary_handle (void) {
//    int16_t counter;
//
//    if (!rotary_flag) return 0;
//    counter = rotary_counter;
//    rotary_counter = 0;
//    rotary_flag = 0;
//    return counter;
//}


//void rotary_irq (void) {
//    static uint8_t position = 1;
//
//     if (ROTARY_CLK_PIN != position) {
//        if (ROTARY_DT_PIN != position) {
//            rotary_counter++;
//        }
//        else {
//            rotary_counter--;
//        }
//     }
//    position = ROTARY_CLK_PIN;
//    rotary_flag = 1;
//}


