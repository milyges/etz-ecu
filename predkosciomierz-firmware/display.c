#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "display.h"

static void display_out(uint8_t byte) {
	if (byte & 0x01)
		DISPLAY_D4_PORT |= (1 << DISPLAY_D4_PIN);
	else
		DISPLAY_D4_PORT &= ~(1 << DISPLAY_D4_PIN);
	
	if (byte & 0x02)
		DISPLAY_D5_PORT |= (1 << DISPLAY_D5_PIN);
	else
		DISPLAY_D5_PORT &= ~(1 << DISPLAY_D5_PIN);
	
	if (byte & 0x04)
		DISPLAY_D6_PORT |= (1 << DISPLAY_D6_PIN);
	else
		DISPLAY_D6_PORT &= ~(1 << DISPLAY_D6_PIN);
	
	if (byte & 0x08)
		DISPLAY_D7_PORT |= (1 << DISPLAY_D7_PIN);
	else
		DISPLAY_D7_PORT &= ~(1 << DISPLAY_D7_PIN);
}

static void display_write(uint8_t byte) {
	DISPLAY_E_PORT |= (1 << DISPLAY_E_PIN);
	display_out(byte >> 4);
	DISPLAY_E_PORT &= ~(1 << DISPLAY_E_PIN);
	DISPLAY_E_PORT |= (1 << DISPLAY_E_PIN);
	display_out(byte & 0xF);
	DISPLAY_E_PORT &= ~(1 << DISPLAY_E_PIN);
	_delay_ms(5);
}

static void display_write_cmd(uint8_t cmd) {
	DISPLAY_RS_PORT &= ~(1 << DISPLAY_RS_PIN);
	display_write(cmd);
}

static void display_write_data(uint8_t data) {
	DISPLAY_RS_PORT |= (1 << DISPLAY_RS_PIN);
	display_write(data);
}

#if 0
void display_clear(void) {
	display_write_cmd(HD44780_CLEAR); // czyszczenie zawartosæi pamieci DDRAM
}
#endif
void display_puts(char * s) {
	while(*s)
		display_write_data(*s++);
}

void display_putc(char c) {
	display_write_data(c);
}

void display_goto(int x, int y) {
	display_write_cmd(HD44780_DDRAM_SET | (x + (0x40 * y)));
}

void display_init(void) {
	int i;
	
	/* Konfiguracja I/O */
	DISPLAY_RS_DDR |= (1 << DISPLAY_RS_PIN);
	DISPLAY_E_DDR |= (1 << DISPLAY_E_PIN);
	DISPLAY_D4_DDR |= (1 << DISPLAY_D4_PIN);
	DISPLAY_D5_DDR |= (1 << DISPLAY_D5_PIN);
	DISPLAY_D6_DDR |= (1 << DISPLAY_D6_PIN);
	DISPLAY_D7_DDR |= (1 << DISPLAY_D7_PIN);
	
	/* Zerujemy RS, RW i EN */
	DISPLAY_E_PORT &= ~(1 << DISPLAY_E_PIN);
	DISPLAY_RS_PORT &= ~(1 << DISPLAY_RS_PIN);
	
	for (i = 0; i < 3; i++) 	{
		DISPLAY_E_PORT |= (1 << DISPLAY_E_PIN);
		display_out(0x03); /* Tryb 8bit */
		DISPLAY_E_PORT &= ~(1 << DISPLAY_E_PIN);
		_delay_ms(5);
	}
	
	DISPLAY_E_PORT |= (1 << DISPLAY_E_PIN);
	display_out(0x02); /* Tryb 4bit */
	DISPLAY_E_PORT &= ~(1 << DISPLAY_E_PIN);	
	_delay_ms(1);
	
	display_write_cmd(HD44780_FUNCTION_SET | HD44780_FONT5x7 | HD44780_TWO_LINE | HD44780_4_BIT); // interfejs 4-bity, 2-linie, znak 5x7
	display_write_cmd(HD44780_DISPLAY_ONOFF | HD44780_DISPLAY_OFF); // wyłączenie wyswietlacza
	display_write_cmd(HD44780_CLEAR); // czyszczenie zawartosæi pamieci DDRAM
	display_write_cmd(HD44780_ENTRY_MODE | HD44780_EM_SHIFT_CURSOR | HD44780_EM_INCREMENT);// inkrementaja adresu i przesuwanie kursora
	display_write_cmd(HD44780_DISPLAY_ONOFF | HD44780_DISPLAY_ON | HD44780_CURSOR_OFF | HD44780_CURSOR_NOBLINK); // w³¹cz LCD, bez kursora i mrugania
}
#if 0
void display_off(void) {
	display_write_cmd(HD44780_DISPLAY_ONOFF | HD44780_DISPLAY_OFF); // wyłączenie wyswietlacza
}
 
void display_setchar(char c, uint8_t * data) {
	int i;
	display_write_cmd(HD44780_CGRAM_SET | (c << 3));
	
	for(i = 0; i < 8; i++)
		display_write_data(data[i]);
}
#endif