#ifndef __DISPLAY_H
#define __DISPLAY_H

/* Podłączenia wyświetlacza */
#define DISPLAY_RS_DDR      DDRA
#define DISPLAY_RS_PORT     PORTA
#define DISPLAY_RS_PIN      PA0

#define DISPLAY_E_DDR       DDRD
#define DISPLAY_E_PORT      PORTD
#define DISPLAY_E_PIN       PD6

#define DISPLAY_D4_DDR      DDRB
#define DISPLAY_D4_PORT     PORTB
#define DISPLAY_D4_PIN      PB4

#define DISPLAY_D5_DDR      DDRB
#define DISPLAY_D5_PORT     PORTB
#define DISPLAY_D5_PIN      PB5

#define DISPLAY_D6_DDR      DDRB
#define DISPLAY_D6_PORT     PORTB
#define DISPLAY_D6_PIN      PB6

#define DISPLAY_D7_DDR      DDRB
#define DISPLAY_D7_PORT     PORTB
#define DISPLAY_D7_PIN      PB7

#define HD44780_CLEAR					0x01

#define HD44780_HOME					0x02

#define HD44780_ENTRY_MODE				0x04
	#define HD44780_EM_SHIFT_CURSOR		0
	#define HD44780_EM_SHIFT_DISPLAY	1
	#define HD44780_EM_DECREMENT		0
	#define HD44780_EM_INCREMENT		2

#define HD44780_DISPLAY_ONOFF			0x08
	#define HD44780_DISPLAY_OFF			0
	#define HD44780_DISPLAY_ON			4
	#define HD44780_CURSOR_OFF			0
	#define HD44780_CURSOR_ON			2
	#define HD44780_CURSOR_NOBLINK		0
	#define HD44780_CURSOR_BLINK		1

#define HD44780_DISPLAY_CURSOR_SHIFT	0x10
	#define HD44780_SHIFT_CURSOR		0
	#define HD44780_SHIFT_DISPLAY		8
	#define HD44780_SHIFT_LEFT			0
	#define HD44780_SHIFT_RIGHT			4

#define HD44780_FUNCTION_SET			0x20
	#define HD44780_FONT5x7				0
	#define HD44780_FONT5x10			4
	#define HD44780_ONE_LINE			0
	#define HD44780_TWO_LINE			8
	#define HD44780_4_BIT				0
	#define HD44780_8_BIT				16

#define HD44780_CGRAM_SET				0x40

#define HD44780_DDRAM_SET				0x80

void display_clear(void);
void display_puts(char * s);
void display_putc(char c);
void display_setchar(char c, uint8_t * data);
void display_goto(int x, int y);
void display_init(void);
void display_on(void);
void display_off(void);

#endif /* __DISPLAY_H */
