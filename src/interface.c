#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "Descriptors.h"
#include "interface.h"
#include "map.h"
#include "immo.h"
#include "params.h"
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#define DATA_BUFSZ            512

static FILE _stdout;
static uint8_t _is_connected = 0;

static inline uint8_t hex2int8(uint8_t * data) {
	uint8_t res = 0;
	
	if ((data[0] >= '0') && (data[0] <= '9')) {
		res = (data[0] - '0') << 4;
	}
	else if ((toupper(data[0]) >= 'A') && (toupper(data[0]) <= 'F')) {
		res = (toupper(data[0]) - 'A' + 10) << 4;
	}
	
	
	if ((data[1] >= '0') && (data[1] <= '9')) {
		res |= data[1] - '0';
	}
	else if ((toupper(data[1]) >= 'A') && (toupper(data[1]) <= 'F')) {
		res |= toupper(data[1]) - 'A' + 10;
	}
	
	return res;
}

static inline uint16_t hex2int16(uint8_t * data) {
	return (hex2int8(data) << 8) | hex2int8(&data[2]);	
}

static USB_ClassInfo_CDC_Device_t _CDC_Interface = {
	.Config = {
			.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
			.DataINEndpoint           = {
					.Address          = CDC_TX_EPADDR,
					.Size             = CDC_TXRX_EPSIZE,
					.Banks            = 1,
				},
			.DataOUTEndpoint = {
					.Address          = CDC_RX_EPADDR,
					.Size             = CDC_TXRX_EPSIZE,
					.Banks            = 1,
				},
			.NotificationEndpoint = {
					.Address          = CDC_NOTIFICATION_EPADDR,
					.Size             = CDC_NOTIFICATION_EPSIZE,
					.Banks            = 1,
				},
		},
};

void EVENT_USB_Device_ControlRequest(void) {
	CDC_Device_ProcessControlRequest(&_CDC_Interface);
}

void EVENT_USB_Device_Connect(void) {
	_is_connected = 1;
}

void EVENT_USB_Device_Disconnect(void) {
	_is_connected = 0;
}

void EVENT_USB_Device_ConfigurationChanged(void) {
	CDC_Device_ConfigureEndpoints(&_CDC_Interface);
}

void interface_init(void) {
	/* Wyłączamy watchdoga */
	MCUSR &= ~(1 << WDRF); 
	wdt_disable();
	
	USB_Init();
	CDC_Device_CreateStream(&_CDC_Interface, &_stdout);
	stdout = &_stdout;
}

static uint8_t interface_exec(uint8_t * data, uint16_t datasz) {
	extern volatile int16_t __timming_advance; 
	extern volatile int16_t __crank_acceleration;
	extern volatile uint16_t __rpm;
	extern volatile uint16_t __throttle_state;
	
	int i, col, row;
	
	if (data[0] == 'v') { /* Nazwa i wersja softu */
		printf("\r\nMZ ECU, firmware version "FW_VERSION);
		return 0;
	}
	else if (data[0] == 'd') { /* Odczyt aktualnych danych */
		printf("\r\n%u %d %d %u", __rpm, __timming_advance, __crank_acceleration, __throttle_state);
		return 0;
	}
	else if (data[0] == 'g') { /* Odczyt parametru konfiguracji z eeprom */
		i = hex2int8(&data[1]);
		if (i >= PARAM_COUNT)
			return 0x01;
		printf("\r\n%04x", __params[i]);
		return 0x00;
	}
	else if (data[0] == 's') { /* Zapis parametru konfiguracji do eeprom */
		i = hex2int8(&data[1]);
		if (i >= PARAM_COUNT)
			return 0x01;
		__params[i] = hex2int16(&data[3]);
		params_save();
		return 0x00;
	}
	else if (data[0] == 'r') { /* Odczyt mapy zapłonu */
		
		for(row = 0; row < MAP_COUNT; row++) {
			for(col = 0; col < MAP_RPM_SIZE; col++) {
				printf("%d ", __ignition_map[row][col]);
			}
				
			putchar(';'); putchar(' ');
		}
		return 0;
	}
	else if (data[0] == 'w') { /* Zapis mapy zapłonu */
		col = 0; row = 0;
		
		for(i = 1; i < datasz; i++) {
			if (data[i] == ';') { /* Następny wiersz */
				row++;
				col = 0;
			}
			else {
				if ((row >= MAP_COUNT) || (col >= MAP_RPM_SIZE)) { /* Mapa za duża, coś skopane */
					return 0x01;
				}
				
				__ignition_map[row][col] = hex2int8(&data[i++]);				
				col++;
			}
		}
		
		/* Zapis do eeprom */
		map_write();
		return 0x00;
	}
	else if (data[0] == 'k') { /* Odczyt kodów immobilizera */
		putchar('\r'); putchar('\n');			
		for(i = 0; i < IMMO_KEYS; i++) {
			printf("%s ", __immo_keys[i]);
		}		
		return 0x00;
	}
	else if (data[0] == 'i') { /* Zapis kodów do immobilizera */
		i = 0;
		row = 0;		
		for(row = 0; row < IMMO_KEYS; row++) {
			col = 0;
			while(++i < datasz) {
				if ((data[i] == ' ') || (data[i] == '\n') || (data[i] == '\0') || (col >= IMMO_KEY_LEN)) {
					break;
				}
				__immo_keys[row][col++] = data[i];
			}
		}
		
		immo_keys_save();
		return 0x00;
	}
	
	return 0xFF; /* Nie ma takiego polecenia */	
}

static void interface_recv_byte(uint8_t data) {
	static uint16_t bufidx;
	static uint8_t buf[DATA_BUFSZ];
	uint8_t err;
	
	if (data == '\n') { /* \n pomijamy */
		return;
	}
	
	else if (data == '\r') { /* Koniec poecenia */
		/* Jeżeli polecenie nie jest puste, wykonujemy je */
		if (bufidx > 0)
			err = interface_exec(buf, bufidx);
		else
			err = 0;
		
		/* Czyścimy bufor */
		bufidx = 0;
		memset(buf, 0x00, DATA_BUFSZ);
		
		/* Wypisujemy kod błędu + prompt */
		printf("\r\n%02x>", err);
	}
	else { /* Normalne dane */
		if (bufidx < DATA_BUFSZ) buf[bufidx++] = data;
	}
}

void interface_loop(void) {
	int16_t data;	
	
	if (_is_connected) { /* Jeżeli urządzenie jest podłączone do komputera, czytamy dane z USB */		
		data = CDC_Device_ReceiveByte(&_CDC_Interface);
		
		if (data >= 0) { /* Sa nowe dane */
			interface_recv_byte(data);
		}
	}	
	
	CDC_Device_USBTask(&_CDC_Interface);
	USB_USBTask();
}
