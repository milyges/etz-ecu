#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "interface.h"
#include "immo.h"
#include "map.h"
#include "params.h"

/* Definicje portów I/O */
#define IGN_COIL_DDR        DDRB
#define IGN_COIL_PORT       PORTB
#define IGN_COIL_PINNO      PB3
#define IGN_COIL_ON()       IGN_COIL_PORT |= (1 << IGN_COIL_PINNO)
#define IGN_COIL_OFF()      IGN_COIL_PORT &= ~(1 << IGN_COIL_PINNO)
#define IGN_COIL_STATE()    ((IGN_COIL_PORT & (1 << IGN_COIL_PINNO)) == (1 << IGN_COIL_PINNO))

#define IDLE_SERVO_DDR      DDRB
#define IDLE_SERVO_PORT     PORTB
#define IDLE_SERVO_PINNO    PB5
#define IDLE_SERVO_OC      

#define START_SERVO_DDR     DDRB
#define START_SERVO_PORT    PORTB
#define START_SERVO_PINNO   PB6
#define START_SERVO_OC     

#define CRANK_SENSOR_DDR    DDRD
#define CRANK_SENSOR_PORT   PORTD
#define CRANK_SENSOR_PIN    PIND
#define CRANK_SENSOR_PINNO0 PD0
#define CRANK_SENSOR_PINNO1 PD1

#define TEMP_SENSOR_DDR     DDRD
#define TEMP_SENSOR_PORT    PORTD
#define TEMP_SENSOR_PINNO   PD4
#define TEMP_SENSOR_ADC     ADC8

#define THROTTLE_DDR        DDRD
#define THROTTLE_PORT       PORTD
#define THROTTLE_PINNO      PD6
#define THROTTLE_ADC        ADC9

#define LAST_ROTATION_TIMES 8 /* Ilość ostatnich połówek z których liczymy średnią */

volatile int16_t __timming_advance = 0; /* Rzeczywiste wyprzedzenie zapłonu */
volatile int16_t __crank_acceleration = 0;
volatile uint16_t __rpm = 0;
uint16_t __throttle_state = 0;
static uint16_t _coil_off_time;
static uint16_t _half_times[LAST_ROTATION_TIMES]; /* Ostatnie czasy połówek obrotów */
static uint16_t _last_half_time_idx = 0; /* Ostatni czas 1/2 obrotu */
static uint16_t _half_time = 0; /* Uśredniony czas 1/2 obrotu */
static uint8_t _ignition_cut_off = 0; /* Zapłon odcięty (zbyt wysokie obroty) */
static uint8_t _dynamic_timming = 0; /* Dunamiczna mapa zapłonu włączona */
static uint16_t _stop_timer = 0; /* "Zegarek" liczący jak długo wał się nie kręci */

/* Obliczenia wykonywane w GMP i DMP */
static inline void _crank_isr_common(void) {
	uint32_t tmp;
	uint8_t i;
	
	/* Zapisujemy czas 1/2 obrotu */
	_last_half_time_idx = (_last_half_time_idx + 1) % LAST_ROTATION_TIMES;	
	_half_times[_last_half_time_idx] = TCNT1; 
	TCNT1 = 0; /* Zerujemy timer0 */	
	
	if (!_half_time) { /* Wał rusza */
		_stop_timer = 0;
		_half_time = _half_times[_last_half_time_idx];
		for(i = 0; i < LAST_ROTATION_TIMES; i++)
			_half_times[i] = _half_time;
	}
	else {
		/* Obliczamy średnią czasów i przyspieszenie */
		__crank_acceleration = _half_time;
		tmp = 0;
		for(i = 0; i < LAST_ROTATION_TIMES; i++)
			tmp += _half_times[i];
		
		_half_time = tmp / LAST_ROTATION_TIMES;
		__crank_acceleration -= _half_time;
	}
}

/* INT1 - przerwanie z czujnika położeniu wału (wał w GMP) */
ISR(INT1_vect) {
	TCNT3 = 0;
	if (IGN_COIL_STATE()) {
		IGN_COIL_OFF(); /* Wyłączamy zasilanie cewki zapłonowej (jeżeli nie było iskry wcześniej - zapłon na pewno nie wypadnie) */
		_coil_off_time = TCNT1;
	}
	
	_crank_isr_common();
	
	/* Odcięcie zapłonu */
	if ((__rpm > __params[PARAM_IGN_CUT_OFF_START]) && (!_ignition_cut_off)) {
		_ignition_cut_off = 1;
	}
	else if ((_ignition_cut_off) && (__rpm < __params[PARAM_IGN_CUT_OFF_END])) {
		_ignition_cut_off = 0;
	}
	
	/* Włączanie / wyłaczanie mapy */
	if ((__rpm > __params[PARAM_DYNAMIC_ON]) && (!_dynamic_timming)) {
		_dynamic_timming = 1;
	}
	else if ((__rpm < __params[PARAM_DYNAMIC_OFF]) && (_dynamic_timming)) {
		_dynamic_timming = 0;
	}

	if ((_ignition_cut_off) || (!_dynamic_timming)) {
		__timming_advance = __params[PARAM_CRANK_OFFSET];
	}
	else {
		/* Obliczamy rzeczywiste wyprzedzenie zapłonu (do celów informacyjnych) */
		__timming_advance = (180UL * (_half_time - _coil_off_time) / _half_time) + __params[PARAM_CRANK_OFFSET];
	}
}

/* INT0 - przerwanie z czujnika położeniu wału (wał w DMP) */
ISR(INT0_vect) {
	uint32_t tmp;
	
	_crank_isr_common();
	
	if (!_half_time)
		return;
	
	/* Obliczamy obroty / minute */
	__rpm = ((60UL * (F_CPU / 64)) / _half_time) >> 1;
		
	if ((!_ignition_cut_off) && (__rpm > 0) && (!__immo_locked)) {
		
		if (_dynamic_timming) { /* Mapa zapłonu włączona */
			/* Obliczamy kiedy ma być iskra */
			if (__ignition_map[__params[PARAM_CURRENT_MAP]][__rpm / 500] <= __params[PARAM_CRANK_OFFSET]) { /* wyprzedzenie mniejsze niż bazowe - nie jesteśmy w stanie tego zrobić */
				TCNT3 = 0;
			}
			else {
				tmp = ((uint32_t)(_half_time + __crank_acceleration) * (uint32_t)(__ignition_map[__params[PARAM_CURRENT_MAP]][__rpm / 500] - __params[PARAM_CRANK_OFFSET])) / 180UL;
				TCNT3 = 0xFFFF - _half_time - __crank_acceleration + tmp;
			}
		}
		else {
			TCNT3 = 0;
		}

		IGN_COIL_ON();
	}
	
	TCNT3 += TCNT1; /* Korekta o czas wykonywania kodu przerwania */
}

ISR(TIMER1_OVF_vect) { /* Przepełnia się gdy nie ma impulsu (wał się nie kręci) */
	_stop_timer++; /* Zwiększamy timer stopu */
	
	if (_stop_timer > 10) { /* Po kilkunastu sekundach wyłączamy zasilanie cewki, aby nie marnowała prądu i się nie grzała niepotrzebnie */
		IGN_COIL_OFF(); 
	}
	
	_half_time = 0;
	__rpm = 0;
	_ignition_cut_off = 0;
}

ISR(TIMER3_OVF_vect) { /* Przerwanie timera sterujacego cewką zapłonową */
	if ((!_half_time) || (_ignition_cut_off) || (__immo_locked)) {
		return;
	}
	
	/* Wyłączamy zasilanie cewki i zapisujemy czas */
	IGN_COIL_OFF();
	_coil_off_time = TCNT1;
}

void init(void) {	
	/* Wyłączamy dzielnik zegara */
	clock_prescale_set(clock_div_1);
	
	/* Niepodłączone piny jako wyjścia */
	DDRB |= (1 << PB0) | (1 << PB2) | (1 << PB4) | (1 << PB7);
	DDRC |= (1 << PC6) | (1 << PC7);
	DDRD |= (1 << PD5) | (1 << PD7);
	DDRE |= (1 << PE6);
	DDRF |= (1 << PF0) | (1 << PF1) | (1 << PF4) | (1 << PF5) | (1 << PF6) | (1 << PF7);
	
	/* Konfiguracja I/O */
	IGN_COIL_DDR |= (1 << IGN_COIL_PINNO);
	IDLE_SERVO_DDR |= (1 << IDLE_SERVO_PINNO);
	START_SERVO_DDR |= (1 << START_SERVO_PINNO);
	CRANK_SENSOR_DDR &= ~((1 << CRANK_SENSOR_PINNO0) | (1 << CRANK_SENSOR_PINNO1)) ;
	TEMP_SENSOR_DDR &= ~(1 << TEMP_SENSOR_PINNO);
	THROTTLE_DDR &= ~(1 << THROTTLE_PINNO);	
	DIDR2 |= (1 << ADC9D) | (1 << ADC8D);
	
	/* Parametry modułu */
	params_init();
	
	/* Inicjalizacja interfejsu USB */
	interface_init();
	
	/* Inicjalizacja immobilizera */
	immo_init();	
	
	/* Mapa zapłonu */
	map_init();
	
	/* INT0, aktywacja zboczem opadającym */
	EICRA &= ~(1 << ISC00);
	EICRA |= (1 << ISC01);
	EIMSK |= (1 << INT0);
	
	/* INT1, aktywacja zboczem narastającym */
	EICRA |= (1 << ISC11) | (1 << ISC10);
	EIMSK |= (1 << INT1);
	
	/* Timer 1 - odmierzanie czasu między impulsami */
	TCCR1B |= (1 << CS11) | (1 << CS10);
	TIMSK1 |= (1 << TOIE1);

	/* Timer 3 - wyzwalanie iskry, taki sam preskaler */
	TCCR3B |= (1 << CS31) | (1 << CS30);
	TIMSK3 |= (1 << TOIE3);

	sei();
}

void write_to_eeprom(void) {
	
}

#if 0
int16_t read_temp(void) {
	uint32_t data = 0;
	
	/* AREF = internal 2.56V, channel = 8 */
	ADMUX = (1 << REFS0) | (1 << REFS1);
	ADCSRB = (1 << MUX5);
	
	_delay_us(1);
	ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIF) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	while(ADCSRA & (1 << ADSC));  /* Czekamy na zakonczenie konwersij */
	ADCSRA = (1 << ADIF);
		
	data = ADC;
	__throttle_state = data;
	return ((data * 380UL) / 1024UL) - 80;
}

void read_throttle_state(void) {
	uint16_t data = 0;
	
	/* AREF = internal 2.56V, channel = 9 */
	ADCSRB = (1 << ADHSM) | (1 << MUX5);
	ADMUX = (1 << REFS0) | (0 << REFS1) | (1 << MUX0);	
	
	_delay_us(1);
	ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIF) | (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0);
	while(ADCSRA & (1 << ADSC));  /* Czekamy na zakonczenie konwersij */
	ADCSRA = (1 << ADIF);
		
	data = ADC;
	__throttle_state = data;
}
#endif
int main(void) {	
	_delay_ms(100);
	
	init();
	
	while(1) {
		//read_throttle_state();
		//read_temp();
		
		interface_loop();
	}
	return 0;
}
