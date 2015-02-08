#ifndef __PARAMS_H
#define __PARAMS_H

#include <stdint.h>

#define PARAM_IGN_CUT_OFF_START  0
#define PARAM_IGN_CUT_OFF_END    1
#define PARAM_DYNAMIC_ON         2
#define PARAM_DYNAMIC_OFF        3
#define PARAM_CURRENT_MAP        4
#define PARAM_COUNT              5

extern uint16_t __params[PARAM_COUNT];

void params_init(void);
void params_save(void);

#endif /* __PARAMS_H */
