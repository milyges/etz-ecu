#ifndef __MAP_H
#define __MAP_H

#include <stdint.h>

#define MAP_RPM_SIZE          16
#define MAP_COUNT             4  /* Ilość map zapisanych w pamięci */

extern uint8_t __ignition_map[MAP_COUNT][MAP_RPM_SIZE];

void map_init(void);
void map_write(void);

#endif /* __MAP_H */
