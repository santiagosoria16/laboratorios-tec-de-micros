#ifndef F_CPU
#define F_CPU 16000000UL // Frecuencia de reloj de 16 MHz
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// Definición de Pines de Hardware
// -----------------------------------------------------------------------------
#define WS2812_PORT PORTB
#define WS2812_DDR  DDRB
#define WS2812_PIN  PB0      // Pin 8 (PB0) para la Matriz LED RGB WS2812B

#define BUZZER_PORT PORTD
#define BUZZER_DDR  DDRD
#define BUZZER_PIN  PD3      // Pin 3 (PD3 / OC2B) para el Buzzer

#define JOY_SW_PORT PORTD
#define JOY_SW_PINR PIND
#define JOY_SW_DDR  DDRD
#define JOY_SW_BIT  PD2      // Pin 2 (PD2) para el Pulsador SW del Joystick
