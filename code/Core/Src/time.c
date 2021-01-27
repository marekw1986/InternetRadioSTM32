/*
 * time.c
 *
 *  Created on: 27 sty 2021
 *      Author: marek
 */

#include "time.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim4;

void delay_us (uint16_t us) {
	__HAL_TIM_SET_COUNTER(&htim4,0);  // set the counter value a 0
	while (__HAL_TIM_GET_COUNTER(&htim4) < us);  // wait for the counter to reach the us input in the parameter
}
