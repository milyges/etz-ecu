#include <avr/eeprom.h>
#include "params.h" 

uint16_t __params[PARAM_COUNT];
static uint16_t _ee_params[PARAM_COUNT] EEMEM; /* Mapa zapisana w eeprom */


void params_init(void) {
	eeprom_busy_wait();
	eeprom_read_block(__params, _ee_params, sizeof(uint16_t) * PARAM_COUNT);
}

void params_save(void) {
	eeprom_busy_wait();
	eeprom_update_block(__params, _ee_params, sizeof(uint16_t) * PARAM_COUNT);
}
