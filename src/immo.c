#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include "immo.h"
#include "params.h"

#define IMMO_LIGHT_DDR      DDRB
#define IMMO_LIGHT_PORT     PORTB
#define IMMO_LIGHT_PINNO    PB1
#define IMMO_LIGHT_ON()     IMMO_LIGHT_PORT |= (1 << IMMO_LIGHT_PINNO)
#define IMMO_LIGHT_OFF()    IMMO_LIGHT_PORT &= ~(1 << IMMO_LIGHT_PINNO)
#define IMMO_LIGHT_TOGGLE() IMMO_LIGHT_PORT ^= (1 << IMMO_LIGHT_PINNO)

#define USART_BAUDRATE      9600
#define USART_UBR           (F_CPU / USART_BAUDRATE / 16 - 1)

uint8_t __immo_locked;
uint8_t __immo_keys[IMMO_KEYS][IMMO_KEY_LEN + 1]; /* = {
	{ "0D00857241BB" },
	{ "000000000000" },
};*/

static uint8_t _ee_immo_keys[IMMO_KEYS][IMMO_KEY_LEN + 1] EEMEM;

ISR(USART1_RX_vect) {
	uint8_t c = UDR1;
	static uint8_t bufidx = 0;
	static uint8_t buf[IMMO_KEY_LEN + 1] = { 0, };
	
	if (!__immo_locked) /* Jeżeli immo nie zablokowane, olewamy odczyty */
		return;
	
	switch(c) {
		case 0x02: { /* Początek nowego odczytu */
			bufidx = 0;
			break;
		}
		case 0x03: { /* Koniec odczytu */
			buf[bufidx] = '\0';
			
			if (!strcmp((char *)buf, (char *)__immo_keys[0])) {
				__immo_locked = 0;
				IMMO_LIGHT_OFF();
				return;
			}
			
			break;
		}
		default: {
			buf[bufidx++] = c;
		}
	}	
}

void immo_keys_save(void) {
	eeprom_busy_wait();
	eeprom_update_block(__immo_keys, _ee_immo_keys, IMMO_KEYS * (IMMO_KEY_LEN + 1));
}

void immo_init(void) {
	eeprom_busy_wait();
	eeprom_read_block(__immo_keys, _ee_immo_keys, IMMO_KEYS * (IMMO_KEY_LEN + 1));
	
	IMMO_LIGHT_DDR |= (1 << IMMO_LIGHT_PINNO);	
	
	UCSR1B = (1 << TXEN1) | (1 << RXEN1) | (1 << RXCIE1);
	UCSR1C = (3 << UCSZ10);
	UBRR1H = (USART_UBR >> 8);
	UBRR1L = USART_UBR & 0xFF;
	
	if (__params[PARAM_IMMO_ENABLED]) {
		__immo_locked = 1;
		IMMO_LIGHT_ON();
	}
	else {
		__immo_locked = 0;
		IMMO_LIGHT_OFF();
	}
}
