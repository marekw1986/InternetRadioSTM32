#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "hd44780.h"
#include "i2c.h"
#include "common.h"
#include "main.h"
#include "cmsis_os.h"

#define LCD_BUF_SIZE 128
#define LCD_BUF_MASK ( LCD_BUF_SIZE - 1)

#define SET_RS 	mpxLCD |= (1<<LCD_RS); 	SEND_I2C
#define CLR_RS 	mpxLCD &= ~(1<<LCD_RS); SEND_I2C

#define SET_RW 	mpxLCD |= (1<<LCD_RW); 	SEND_I2C
#define CLR_RW 	mpxLCD &= ~(1<<LCD_RW); SEND_I2C

#define SET_E 	mpxLCD |= (1<<LCD_E); 	SEND_I2C
#define CLR_E 	mpxLCD &= ~(1<<LCD_E); 	SEND_I2C

#define SET_BACKLIGHT mpxLCD |= (1<<LCD_BACKLIGHT); SEND_I2C
#define CLR_BACKLIGHT mpxLCD &= ~(1<<LCD_BACKLIGHT); SEND_I2C

#define LCD_HOME    0x00
#define LCD_CLS     0x01
#define LCD_LOCATE  0x02

uint8_t  LCD_Buf[LCD_BUF_SIZE];
uint8_t LCD_Head;
uint8_t LCD_Tail;

static void lcd_buf_put(uint8_t data);
static uint8_t lcd_buf_get(void);
void lcd_write_cmd(uint8_t cmd);
void lcd_write_data(uint8_t data);


static void lcd_buf_put(uint8_t data) {
    uint8_t tmp_head;
    
    tmp_head = ( LCD_Head + 1) & LCD_BUF_MASK;
    if ( tmp_head == LCD_Tail ) {
        LCD_Head = LCD_Tail;
    }
    else {
        LCD_Head = tmp_head;
        LCD_Buf[tmp_head] = data;
    }
}

static uint8_t lcd_buf_get(void) {
    uint8_t data;
    if ( LCD_Head == LCD_Tail ) return 0;
    LCD_Tail = (LCD_Tail + 1) & LCD_BUF_MASK;
    data = LCD_Buf[LCD_Tail];
    return data;
}

bool lcd_handle(void) {
    uint8_t data = lcd_buf_get();
    
    if (!data) return false;
    if (data & 0x80) {  //It is command for LCD!
        uint8_t cmd = (uint8_t)((data & 0x70) >> 4);
        switch(cmd) {
            case LCD_HOME:
            //lcd_write_cmd( LCDC_CLS|LCDC_HOME );
            #if USE_RW == 0
            osDelay(5);
            #endif            
            break;

            case LCD_CLS:
            lcd_write_cmd( LCDC_CLS );
            #if USE_RW == 0
            osDelay(5);
            #endif
            break;            
            
            case LCD_LOCATE:;
            uint8_t y = (data & 0x0F);
            uint8_t x = lcd_buf_get();
            switch(y) {
                case 0: y = LCD_LINE1; break;
                #if (LCD_ROWS>1)
                case 1: y = LCD_LINE2; break;
                #endif
                #if (LCD_ROWS>2)
                case 2: y = LCD_LINE3; break;
                #endif
                #if (LCD_ROWS>3)
                case 3: y = LCD_LINE4; break;
                #endif
            }
            lcd_write_cmd( (0x80 + y + x) );            
            break;
            
            default:
            
            break;
        }
    }
    else {  //It is just an ASCII character, send it to LCD
        lcd_write_data(data);
    }
    return true;
}

#if USE_RW
uint8_t check_BF(void);
#endif

uint8_t	mpxLCD;

static inline void data_dir_out(void) {
    mpxLCD	&= ~(1<<LCD_D7);
    mpxLCD	&= ~(1<<LCD_D6);
    mpxLCD	&= ~(1<<LCD_D5);
    mpxLCD	&= ~(1<<LCD_D4);
    SEND_I2C;
}

#if USE_RW
static inline void data_dir_in(void) {
    mpxLCD |= (1<<LCD_D7);
    mpxLCD |= (1<<LCD_D6);
    mpxLCD |= (1<<LCD_D5);
    mpxLCD |= (1<<LCD_D4);
    SEND_I2C;
}
#endif

static inline void lcd_sendHalf(uint8_t data) {
    if (data&(1<<0)) mpxLCD |= (1<<LCD_D4); else mpxLCD &= ~(1<<LCD_D4);
    if (data&(1<<1)) mpxLCD |= (1<<LCD_D5); else mpxLCD &= ~(1<<LCD_D5);
    if (data&(1<<2)) mpxLCD |= (1<<LCD_D6); else mpxLCD &= ~(1<<LCD_D6);
    if (data&(1<<3)) mpxLCD |= (1<<LCD_D7); else mpxLCD &= ~(1<<LCD_D7);
    SEND_I2C;
}

#if USE_RW == 1

static inline uint8_t lcd_readHalf(void)
{
	uint8_t result=0;
    uint8_t res=0;

    res = RECEIVE_I2C;
    if(res&(1<<LCD_D4)) result |= (1<<0);
    if(res&(1<<LCD_D5)) result |= (1<<1);
    if(res&(1<<LCD_D6)) result |= (1<<2);
    if(res&(1<<LCD_D7)) result |= (1<<3);

	return result;
}
#endif

void _lcd_write_byte(unsigned char _data) {

	data_dir_out();

	#if USE_RW == 1
		CLR_RW;
	#endif

	SET_E;
	lcd_sendHalf(_data >> 4);
	CLR_E;

	SET_E;
	lcd_sendHalf(_data);
	CLR_E;

	#if USE_RW == 1

		while( (check_BF() & (1<<7)) );

	#else
		osDelay(120);
	#endif

}

#if USE_RW == 1
uint8_t _lcd_read_byte(void) {

	uint8_t result=0;
	data_dir_in();

	SET_RW;

	SET_E;

	result = (lcd_readHalf() << 4);	

	CLR_E;

	SET_E;
	result |= lcd_readHalf();
	CLR_E;

	return result;
}
#endif


#if USE_RW == 1
uint8_t check_BF(void) {

	CLR_RS;

	return _lcd_read_byte();
}
#endif


void lcd_write_cmd(uint8_t cmd) {

	CLR_RS;

	_lcd_write_byte(cmd);
}

void lcd_write_data(uint8_t data) {

	SET_RS;
	_lcd_write_byte(data);
}


void lcd_char(char c) {
	lcd_buf_put( ( c>=0x80 && c<=0x87 ) ? (c & 0x07) : c);
}


void lcd_str(const char * str) {
	register char znak;
	while ( (znak=*(str++)) ) lcd_char( znak );
}

uint16_t lcd_str_part(const char* str, const uint16_t len) {
    register char znak;
    uint16_t cnt = 0;
    while ( (znak=*(str++)) ) {
        lcd_char( znak );
        cnt++;
        if (cnt >= len) {
            break;
        }
    }
    return len-cnt;
}

uint16_t lcd_utf8str_part(const char* str, const uint16_t len) {
    char znak;
    uint16_t cnt=0;
    while ( (znak=*str) ) {
        str++;
        if (znak & (1<<7)) {
            uint16_t znak_utf = znak<<8 | (uint8_t)*str;
            switch (znak_utf) {
                case 0xC485:
                lcd_char('a');
                break;

                case 0xC487:
                lcd_char('c');
                break;
                
                case 0xC499:
                lcd_char('e');
                break;
                
                case 0xC582:
                lcd_char('l');
                break;
                
                case 0xC584:
                lcd_char('n');
                break;
                
                case 0xC3B3:
                lcd_char('o');
                break;
                
                case 0xC59B:
                lcd_char('s');
                break;
                
                case 0xC5BA:
                case 0xC5BC:
                lcd_char('z');
                break;
                
                case 0xC484:
                lcd_char('A');
                break;
                
                case 0xC486:
                lcd_char('C');
                break;
                
                case 0xC498:
                lcd_char('E');
                break;
                
                case 0xC581:
                lcd_char('L');
                break;
                
                case 0xC583:
                lcd_char('N');
                break;
                
                case 0xC393:
                lcd_char('O');
                break;
                
                case 0xC59A:
                lcd_char('S');
                break;
                
                case 0xC5B9:
                case 0xC5BB:
                lcd_char('Z');
                break;
                
                default:
                lcd_char('?');
                break;
            }
            str++;
        }
        else {
            lcd_char(znak);
        }
        cnt++;
        if (cnt >= len) {
            break;
        }
    }
    return len-cnt;
}

void lcd_str_padd_rest(const char* str, const uint16_t len, char padd) {
	uint8_t rest = lcd_str_part(str, LCD_COLS);
	while (rest) {
		lcd_char(padd);
		rest--;
	}
}

void lcd_utf8str_padd_rest(const char* str, const uint16_t len, char padd) {
	uint8_t rest = lcd_utf8str_part(str, LCD_COLS);
	while (rest) {
		lcd_char(padd);
		rest--;
	}
}

void lcd_locate(uint8_t y, uint8_t x) {
    uint8_t tmp;
    
    tmp = 0x80 | (0x70 & (LCD_LOCATE << 4)) | (0x0F & y);
    lcd_buf_put(tmp);
    lcd_buf_put(x);
}


void lcd_cls(void) {
    uint8_t tmp;
    
    tmp = 0x80 | (0x70 & (LCD_CLS << 4));
    lcd_buf_put(tmp);
}


void lcd_home(void) {
    uint8_t tmp;
    
    tmp = 0x80 | (0x70 & (LCD_HOME << 4));
    lcd_buf_put(tmp);
}

void lcd_set_backlight_state(bool state) {
    if (state == true) {
        CLR_BACKLIGHT;
    }
    else {
        SET_BACKLIGHT;
    }
}



void lcd_init( void ) {
    mpxLCD = 0;

    //LCD_LED_ON;

    osDelay(15);
    mpxLCD &= ~(1<<LCD_E);
    mpxLCD &= ~(1<<LCD_RS);
    #if USE_RW == 1
        mpxLCD &= ~(1<<LCD_RW);
    #endif
    SEND_I2C;

	// jeszcze nie mozna uzywac Busy Flag
	SET_E;
	lcd_sendHalf(0x03);	// tryb 8-bitowy
	CLR_E;
	osDelay(5);

	SET_E;
	lcd_sendHalf(0x03);	// tryb 8-bitowy
	CLR_E;
	osDelay(100);

	SET_E;
	lcd_sendHalf(0x03);	// tryb 8-bitowy
	CLR_E;
	osDelay(100);

	SET_E;
	lcd_sendHalf(0x02);// tryb 4-bitowy
	CLR_E;
	osDelay(100);

	// juz mozna uzywac Busy Flag
	// tryb 4-bitowy, 2 wiersze, znak 5x7
	lcd_write_cmd( LCDC_FUNC|LCDC_FUNC4B|LCDC_FUNC2L|LCDC_FUNC5x7 );

	// wylaczenie kursora
	lcd_write_cmd( LCDC_ONOFF|LCDC_CURSOROFF );
	// wlaczenie wyswietlacza
	lcd_write_cmd( LCDC_ONOFF|LCDC_DISPLAYON );
	// przesuwanie kursora w prawo bez przesuwania zawartosci ekranu
	lcd_write_cmd( LCDC_ENTRY|LCDC_ENTRYR );

	// kasowanie ekranu
	//lcd_cls();
    lcd_write_cmd( LCDC_CLS );
    #if USE_RW == 0
    osDelay(5);
    #endif

    LCD_Head = 0;
    LCD_Tail = 0;
}
