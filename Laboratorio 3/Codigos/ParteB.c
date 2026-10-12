#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#define BAUD 9600
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

// Definición de pines del Puente H
#define IN1_PIN PD2
#define IN2_PIN PD3
#define PWM_PIN PD6

// Variable global para el Setpoint de iluminación
volatile uint16_t setpoint_iluminacion = 512;
volatile uint8_t nuevo_sp_flag = 0; // Bandera para avisar al main()

// --- Inicialización de UART ---
void UART_init(unsigned int ubrr) {
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	
	PORTD |= (1 << PORTD0); // Resistencia Pull-Up interna en RX
}

void UART_sendChar(char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void UART_sendString(const char* str) {
	while (*str) {
		UART_sendChar(*str++);
	}
}

// --- Inicialización de ADC ---
void ADC_init(void) {
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
	_delay_us(10);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

// --- Inicialización de PWM ---
void PWM_init(void) {
	DDRD |= (1 << PWM_PIN) | (1 << IN1_PIN) | (1 << IN2_PIN);
	TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
	TCCR0B = (1 << CS01) | (1 << CS00);
	OCR0A = 0;
}
