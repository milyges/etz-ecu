#include <avr/eeprom.h>
#include "map.h"

uint8_t __ignition_map[MAP_COUNT][MAP_RPM_SIZE];

static uint8_t _ee_ignition_map[MAP_COUNT][MAP_RPM_SIZE] EEMEM; /* Mapa zapisana w eeprom */

void map_init(void) {
	eeprom_busy_wait();
	eeprom_read_block(__ignition_map, _ee_ignition_map, MAP_COUNT * MAP_RPM_SIZE);
}

void map_write(void) {
	eeprom_busy_wait();
	eeprom_update_block(__ignition_map, _ee_ignition_map, MAP_COUNT * MAP_RPM_SIZE);
}
