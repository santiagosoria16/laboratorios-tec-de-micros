#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#define CALEFACTOR PB0 
#define VENTILADOR PB1    


#define DHT_PORT PORTC
#define DHT_DDR  DDRC
#define DHT_PIN  PINC
#define DHT_BIT  PC0      


#define SDA PC4           
#define SCL PC5      
#define LCD_ADDR 0x27


#define PM_MAX_PERMITIDO 25
#define PM_MIN_PERMITIDO 10

volatile uint8_t flag_medir = 0;
volatile uint16_t contador_ms = 0;

uint8_t temperatura = 0;
uint8_t humedad = 0;

int8_t punto_medio = 20;
