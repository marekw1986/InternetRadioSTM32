/* 
 * File:   hd44780.h
 * Author: marek
 *
 * Created on 12 maja 2019, 19:41
 */
#include <stdint.h>
#include "main.h"
#include "common.h"

#ifndef HD44780_H
#define	HD44780_H

#define LCD_ROWS 4
#define LCD_COLS 20

#define USE_RW 1
#define PCF8574_LCD_ADDR 0x38

//#define LCD_LED_ON i2c_send_byte(0x0F | (i2c_rcv_byte(PCF8574_IO_ADDR) & 0xE0) | 0x10, PCF8574_IO_ADDR);
//#define LCD_LED_OFF i2c_send_byte(0x0F | (i2c_rcv_byte(PCF8574_IO_ADDR) & 0xE0), PCF8574_IO_ADDR);

#define LCD_D7 	7
#define LCD_D6 	6
#define LCD_D5 	5
#define LCD_D4 	4

#define LCD_RS 	3
#define LCD_RW 	2
#define LCD_E 	1
#define LCD_BACKLIGHT 0

#if ( (LCD_ROWS == 4) && (LCD_COLS == 20) )
#define LCD_LINE1 0x00		// adres 1 znaku 1 wiersza
#define LCD_LINE2 0x28		// adres 1 znaku 2 wiersza
#define LCD_LINE3 0x14  	// adres 1 znaku 3 wiersza
#define LCD_LINE4 0x54  	// adres 1 znaku 4 wiersza
#else
#define LCD_LINE1 0x00		// adres 1 znaku 1 wiersza
#define LCD_LINE2 0x40		// adres 1 znaku 2 wiersza
#define LCD_LINE3 0x10  	// adres 1 znaku 3 wiersza
#define LCD_LINE4 0x50  	// adres 1 znaku 4 wiersza
#endif

#define SEND_I2C 		i2c_send_byte(mpxLCD, PCF8574_LCD_ADDR)

#define RECEIVE_I2C  	i2c_rcv_byte(PCF8574_LCD_ADDR)

#define LCDC_CLS					0x01
#define LCDC_HOME					0x02
#define LCDC_ENTRY					0x04
	#define LCDC_ENTRYR					0x02
	#define LCDC_ENTRYL					0
	#define LCDC_MOVE					0x01
#define LCDC_ONOFF					0x08
	#define LCDC_DISPLAYON				0x04
	#define LCDC_CURSORON				0x02
	#define LCDC_CURSOROFF				0
	#define LCDC_BLINKON				0x01
#define LCDC_SHIFT					0x10
	#define LCDC_SHIFTDISP				0x08
	#define LCDC_SHIFTR					0x04
	#define LCDC_SHIFTL					0
#define LCDC_FUNC					0x20
	#define LCDC_FUNC8B					0x10
	#define LCDC_FUNC4B					0
	#define LCDC_FUNC2L					0x08
	#define LCDC_FUNC1L					0
	#define LCDC_FUNC5x10				0x04
	#define LCDC_FUNC5x7				0
#define LCDC_SET_CGRAM				0x40
#define LCDC_SET_DDRAM				0x80

#ifdef	__cplusplus
extern "C" {
#endif
    
#define lcd_flush_buffer() while(lcd_handle())

void lcd_init(void);
void lcd_cls(void);
void lcd_char(char c);
void lcd_str(const char * str);
uint16_t lcd_str_part(const char* str, const uint16_t len);
uint16_t lcd_utf8str_part(const char* str, const uint16_t len);
void lcd_str_padd_rest(const char* str, const uint16_t len, char padd);
void lcd_utf8str_padd_rest(const char* str, const uint16_t len, char padd);
void lcd_locate(uint8_t y, uint8_t x);
void lcd_home(void);
void lcd_set_backlight_state(bool state);
bool lcd_handle(void);


#ifdef	__cplusplus
}
#endif

#endif	/* HD44780_H */

