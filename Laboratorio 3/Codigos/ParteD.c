#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

// Dirección I2C del PCF8574 (Cambiar a 0x3F si la pantalla LCD no responde)
#define PCF8574_ADDR 0x27

// ==========================================
// DEFINICIÓN DE FRECUENCIAS (Hz)
// ==========================================
#define SIL 0

// Octava 2
#define B2  123

// Octava 3 - Bajos / Acompañamiento
#define C3  131
#define CS3 139
#define D3  147
#define DS3 156
#define EB3 156
#define E3  165
#define F3  175
#define FS3 185
#define G3  196
#define GS3 208
#define AB3 208
#define A3  220
#define AS3 233
#define BB3 233
#define B3  247


