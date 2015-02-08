#ifndef __IMMO_H
#define __IMMO_H

#define IMMO_KEY_LEN        12
#define IMMO_KEYS           2

extern uint8_t __immo_locked;
extern uint8_t __immo_keys[IMMO_KEYS][IMMO_KEY_LEN + 1];

void immo_init(void);
void immo_keys_save(void);

#endif /* __IMMO_H */
