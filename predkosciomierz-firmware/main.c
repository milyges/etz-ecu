#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"

#define SIN_EN_DDR           DDRB
#define SIN_EN_PORT          PORTB
#define SIN_EN_PIN           PB3

#define SIN_PLUS_DDR         DDRD
#define SIN_PLUS_PORT        PORTD
#define SIN_PLUS_PIN         PD0

#define SIN_MINUS_DDR        DDRD
#define SIN_MINUS_PORT       PORTD
#define SIN_MINUS_PIN        PD1

#define COS_EN_DDR           DDRB
#define COS_EN_PORT          PORTB
#define COS_EN_PIN           PB2

#define COS_PLUS_DDR         DDRD
#define COS_PLUS_PORT        PORTD
#define COS_PLUS_PIN         PD4

#define COS_MINUS_DDR        DDRD
#define COS_MINUS_PORT       PORTD
#define COS_MINUS_PIN        PD5

#define IGNITION_STATE_DDR   DDRB
#define IGNITION_STATE_PORT  PORTB
#define IGNITION_STATE_PIN   PINB
#define IGNITION_STATE_PINNO PB0

#define IMPS_PER_100M    810

static uint8_t _sin_value;
static uint8_t _cos_value;

static uint32_t _odometer;
static uint8_t _odometer_meters;
static uint32_t _trip;

static uint32_t _ee_odometer EEMEM;
static uint8_t _ee_odometer_meters EEMEM;
static uint32_t _ee_trip EEMEM;

static uint16_t _imps;
static uint8_t _speed;

static uint16_t _imp_times[4];
static uint8_t _last_time_idx;

static char _stopped;

ISR(TIMER0_OVF_vect) {
	static uint8_t counter = 0;
	
	if (counter <= _sin_value)
		SIN_EN_PORT |= (1 << SIN_EN_PIN);
	else
		SIN_EN_PORT &= ~(1 << SIN_EN_PIN);
	
	if (counter <= _cos_value)
		COS_EN_PORT |= (1 << COS_EN_PIN);
	else
		COS_EN_PORT &= ~(1 << COS_EN_PIN);
		
	
	counter++;
	TCNT0 = 0xFC;
}

ISR(TIMER1_OVF_vect) {
	_speed = 0;
	_imp_times[0] = 0;
	_imp_times[1] = 0;
	_imp_times[2] = 0;
	_imp_times[3] = 0;
	_stopped = 1;
}

ISR(INT0_vect) {
	_trip = 0;
}

ISR(INT1_vect) {
	_imps++;
		
	/* Nie zapisujemy jak dopiero ruszamy bo wychodza pierdoły */
	if (!_stopped) {
		_last_time_idx = (_last_time_idx + 1) % 4;
		_imp_times[_last_time_idx] = TCNT1 - 0x80;
	}
	_stopped = 0;
	TCNT1 = 0x80;
	
	if (_imps >= IMPS_PER_100M) {
		_odometer_meters++;
		if (_odometer_meters >= 10) {
			_odometer++;
			_odometer_meters = 0;
		}
		_trip++;
		if (_trip > 99999)
			_trip = 0;
		_imps = 0;
	}
}

ISR(ANA_COMP_vect) {
		eeprom_busy_wait();
		eeprom_update_byte(&_ee_odometer_meters, _odometer_meters);
		eeprom_busy_wait();
		eeprom_update_dword(&_ee_odometer, _odometer);
		eeprom_busy_wait();
		eeprom_update_dword(&_ee_trip, _trip);
	
		/* Reset procesora */
		cli();
		wdt_enable(WDTO_15MS);
		for(;;);
	
}

void init(void) {
	/* Stan zapłonu */
	IGNITION_STATE_DDR &= ~(1 << IGNITION_STATE_PINNO);
	IGNITION_STATE_PORT &= ~(1 << IGNITION_STATE_PINNO);
	
	/* Tak długo jak nie włączy ktoś zapłonu, zatrzymujemy sie */
	while(!(IGNITION_STATE_PIN & (1 << IGNITION_STATE_PINNO))) ;
	
	_delay_ms(10); /* Chwila na załączenie sie wyświetlacza */
	
	_sin_value = 0;
	_cos_value = 0;
	_speed = 0;
	_imps = 0;
	_last_time_idx = 0;
		
	SIN_EN_PORT &= ~(1 << SIN_EN_PIN);
	SIN_EN_DDR |= (1 << SIN_EN_PIN);
	SIN_PLUS_PORT &= ~(1 << SIN_PLUS_PIN);
	SIN_PLUS_DDR  |= (1 << SIN_PLUS_PIN);
	SIN_MINUS_PORT &= ~(1 << SIN_MINUS_PIN);
	SIN_MINUS_DDR  |= (1 << SIN_MINUS_PIN);
		
	COS_EN_PORT &= ~(1 << COS_EN_PIN);
	COS_EN_DDR |= (1 << COS_EN_PIN);
	COS_PLUS_PORT &= ~(1 << COS_PLUS_PIN);
	COS_PLUS_DDR  |= (1 << COS_PLUS_PIN);
	COS_MINUS_PORT &= ~(1 << COS_MINUS_PIN);
	COS_MINUS_DDR  |= (1 << COS_MINUS_PIN);
	
	/* Timer0  */
	TCCR0B =  (1 << CS01) | (1 << CS00);	
	
	/* Timer 1 */
	TCCR1B = (1 << CS11) | (1 << CS10);
	
	TIMSK = (1 << TOIE0) | (1 << TOIE1);
	
	/* Przerwanie od impulsatora i przycisku */
	DDRD &= ~((1 << PD3) | (1 << PD2));
	PORTD |= (1 << PD3) | (1 << PD2);
	
	MCUCR = (1 << ISC11) | (1 << ISC01);
	GIMSK = (1 << INT1) | (1 << INT0);
	
	sei();
	
	/* Odczyt danych z EEPROM */
	eeprom_busy_wait();
	_odometer = eeprom_read_dword(&_ee_odometer);
	eeprom_busy_wait();
	_trip = eeprom_read_dword(&_ee_trip);
	eeprom_busy_wait();
	_odometer_meters = eeprom_read_byte(&_ee_odometer_meters);
	
	/* Komparator */	
	ACSR = (0 << ACD) | (0 << ACBG) | (0 << ACIC) | (1 << ACIS1) | (0 << ACIS0);
	ACSR |= (1 << ACIE) | (1 << ACI);
	
}

/* NOTE: To nie jest rzeczywisty kąt, przybliżamy sin/cos liniowo */
void set_angle(uint16_t angle) {
	uint16_t tmp;
	
	if (angle <= 90) {		
		COS_MINUS_PORT &= ~(1 << COS_MINUS_PIN);
		COS_PLUS_PORT |= (1 << COS_PLUS_PIN);
		
		SIN_MINUS_PORT &= ~(1 << SIN_MINUS_PIN);
		SIN_PLUS_PORT |= (1 << SIN_PLUS_PIN);
				
		tmp = (angle * 0xFF) / 90;
		_cos_value = tmp;
		_sin_value = 0xFF - _cos_value;
	}
	else if ((angle > 90) && (angle <= 180)) {		
		COS_MINUS_PORT &= ~(1 << COS_MINUS_PIN);
		COS_PLUS_PORT |= (1 << COS_PLUS_PIN);
		
		SIN_PLUS_PORT &= ~(1 << SIN_PLUS_PIN);
		SIN_MINUS_PORT |= (1 << SIN_MINUS_PIN);
		
		tmp = ((angle - 90) * 0xFF) / 90;
		
		_sin_value = tmp;
		_cos_value = 0xFF - _sin_value;
	}
	else /*if ((angle > 180) && (angle <= 270))*/ {
		COS_PLUS_PORT &= ~(1 << COS_PLUS_PIN);
		COS_MINUS_PORT |= (1 << COS_MINUS_PIN);		
		
		SIN_PLUS_PORT &= ~(1 << SIN_PLUS_PIN);
		SIN_MINUS_PORT |= (1 << SIN_MINUS_PIN);
		
		tmp = ((angle - 180) * 0xFF) / 90;
		
		_cos_value = tmp;
		_sin_value = 0xFF - _cos_value;
	}
}

static uint8_t _speed2angle[] = {
	0,   //   0 km/h
	25,  //  10 kmh/h
	41,  //  20 km/h
	57,  //  30 km/h
	74,  //  40 km/h
	97,  //  50 km/h
	118, //  60 km/h
	133, //  70 km/h
	147, //  80 km/h
	161, //  90 km/h
	180, // 100 km/h
	202, // 110 km/h
	218, // 120 km/h
	233, // 130 km/h
	248, // 140 km/h
};

int main(void) {	
	char display_buf[9];// = "      KM";
	//char trip_buf[9];// = "      KM";
	char tmpbuf[7];
	uint16_t imp_time;

	init();
	display_init();
	
	display_buf[6] = 'K';
	display_buf[7] = 'M';
	display_buf[8] = '\0';
	
	while(1) {
		/* Obliczanie prędkości */
		imp_time = (_imp_times[0] + _imp_times[1] + _imp_times[2] + _imp_times[3]) / 4;
		
		if (!imp_time) {
			_speed = 0;
		}
		else {
			_speed = ((F_CPU / 64) / imp_time);
			_speed = _speed * 100 / 225;
		}
		
		/* Obliczamy wychylenie wskazowki */
		char i = _speed / 10;
		char j = _speed % 10;
		char delta = (_speed2angle[i + 1] - _speed2angle[i]);
		
		set_angle(_speed2angle[i] + j * delta / 10);
		
		/* Wyświetlanie przebiegu całkoitego */
		for(i = 0; i < 6; i++) {
			display_buf[i] = ' ';
		}
		
		itoa(_odometer, tmpbuf, 10);
		char len = strlen(tmpbuf);
		for(i = 0; i < len; i++)
			display_buf[6 - len + i] = tmpbuf[i];
			
		display_goto(0, 0);
		display_puts(display_buf);
		
		/* Wyświetlanie przebiegu kasowalnego */
		for(i = 0; i < 6; i++) {
			display_buf[i] = ' ';
		}
		
		itoa(_trip, tmpbuf, 10);
		len = strlen(tmpbuf);
		
		display_buf[4] = '.';
		display_buf[5] = tmpbuf[--len];
		if (len > 0) {
			for(i = 0; i < len; i++)
				display_buf[4 - len + i] = tmpbuf[i];
		}
		else {
			display_buf[3] = '0';
		}
		
		display_goto(0, 1);	
		display_puts(display_buf);
		
	}
	
	return 0;
}

void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));

void wdt_init(void) {
    MCUSR = 0;
    wdt_disable();
	return;
}