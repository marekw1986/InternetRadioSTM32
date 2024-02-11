/*
 * common.c
 *
 *  Created on: 24 sty 2021
 *      Author: marek
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "common.h"

long map(long x, long in_min, long in_max, long out_min, long out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
