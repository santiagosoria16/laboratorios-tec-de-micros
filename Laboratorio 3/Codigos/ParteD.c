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

