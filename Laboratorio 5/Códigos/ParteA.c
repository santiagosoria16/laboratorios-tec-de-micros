#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>

// --- DEFINICIÓN DE PINES ---
#define LED_PIN       PB5   // Pin 13 Arduino
#define BUZZER_PIN    PB0   // Pin 8 Arduino
#define BUTTON_PIN    PD2   // Pin 2 Arduino (INT0)

#define LCD_RS_PIN    PB1   // Pin 9 Arduino
#define LCD_E_PIN     PB2   // Pin 10 Arduino
