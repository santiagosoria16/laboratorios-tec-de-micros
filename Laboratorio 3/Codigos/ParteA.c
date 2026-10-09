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

void uart_init(void)
{
	uint16_t ubrr = (uint16_t)((F_CPU / (16UL * 9600UL)) - 1);

	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;

	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_putchar(char c)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}
