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

void Motor_Control(int direction, uint8_t pwm_val) {
	if (direction > 0) {
		PORTD |= (1 << IN1_PIN);
		PORTD &= ~(1 << IN2_PIN);
		} else if (direction < 0) {
		PORTD &= ~(1 << IN1_PIN);
		PORTD |= (1 << IN2_PIN);
		} else {
		PORTD &= ~((1 << IN1_PIN) | (1 << IN2_PIN));
		pwm_val = 0;
	}
	OCR0A = pwm_val;
}

// --- Interrupción ultrarrápida de recepción UART ---
ISR(USART_RX_vect) {
	static char buffer[10];
	static uint8_t idx = 0;
	char rx_byte = UDR0;

	// Acepta Enter (\n o \r), Espacio o Punto y Coma (;) como finalizador
	if (rx_byte == '\n' || rx_byte == '\r' || rx_byte == ' ' || rx_byte == ';') {
		if (idx > 0) {
			buffer[idx] = '\0';
			int val = atoi(buffer);
			if (val >= 0 && val <= 1023) {
				setpoint_iluminacion = (uint16_t)val;
				nuevo_sp_flag = 1; // Notifica al main
			}
			idx = 0;
		}
		} else if (rx_byte >= '0' && rx_byte <= '9') {
		if (idx < sizeof(buffer) - 1) {
			buffer[idx++] = rx_byte;
		}
		} else {
		idx = 0; // Descarta buffer si entran letras o ruido
	}
}
