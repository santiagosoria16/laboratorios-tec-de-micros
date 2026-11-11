// Define la frecuencia de reloj del microcontrolador (16 MHz)
#define F_CPU 16000000UL
#define BAUD 9600 // UART
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

// Librerías principales de AVR C
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <avr/sleep.h> //  Modos Sleep
#include <avr/wdt.h>   //  Watchdog Timer
#include <avr/power.h> //  Power Reduction Register (PRR)

// --- DEFINICIÓN DE PINES DEL SISTEMA ---
#define LED_PIN              PB5   // Pin 13 Arduino (LED integrado)
#define BUZZER_PIN           PB0   // Pin 8 Arduino (Buzzer pasivo)
#define BUTTON_PIN           PD2   // Pin 2 Arduino (Entrada INT0 - Botón Navegación)
#define EMERGENCY_BUTTON_PIN PD3   // Pin 3 Arduino (Entrada INT1 - Botón Emergencia)

// Pines para las pruebas del WDT
#define WDT_TEST_PIN         PB1   // Pin 9 Arduino: conmuta de nivel en cada interrupción WDT
#define HANG_PIN             PD4   // Pin 4 Arduino (pull-up): a GND simula el bloqueo

// --- MAPEO Y REGISTROS DEL SENSOR DHT11 ---
#define DHT_PORT        PORTB
#define DHT_DDR         DDRB
#define DHT_PIN         PINB
#define DHT_BIT         PB4   // Pin 12 Arduino

// --- CONFIGURACIÓN DEL DISPLAY LCD I2C (EXPANSOR PCF8574) ---
#define I2C_ADDR        0x27
#define LCD_ENABLE      0x04
#define LCD_RS          0x01

