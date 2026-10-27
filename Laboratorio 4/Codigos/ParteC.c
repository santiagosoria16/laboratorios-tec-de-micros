#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// DIRECCIÓN I2C DE LA PANTALLA LCD (Módulo PCF8574)
// Usar 0x4E para dirección de placa 0x27 (0x27 << 1)
// Si la pantalla enciende pero no muestra letras, cambiar 0x4E por 0x7E (0x3F << 1)
// ============================================================================
#define LCD_ADDR 0x4E

// Definición de bits de Puerto B (Salidas)
#define BUZZER_PIN   PB1  // Pin Digital 9 (Señal 'S' del Buzzer)
#define LED_VERDE    PB2  // Pin Digital 10
#define LED_ROJO     PB3  // Pin Digital 11
