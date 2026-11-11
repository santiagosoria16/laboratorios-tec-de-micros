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
