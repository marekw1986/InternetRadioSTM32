/*
 * common.h
 *
 *  Created on: 24 sty 2021
 *      Author: marek
 */

#ifndef INC_COMMON_H_
#define INC_COMMON_H_

#define millis() HAL_GetTick()

typedef enum {false = 0, true} bool;

long map(long x, long in_min, long in_max, long out_min, long out_max);

#endif /* INC_COMMON_H_ */
