#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

// ---------------- Configuración ----------------
#define BAUD            115200UL
#define UBRR_VALUE      ((F_CPU / (8UL * BAUD)) - 1)   // con U2X0 = 1 (error ~2.1 %)

#define OVERSAMPLE      16          // 16 * 1023 = 16368 -> entra en uint16_t
#define USAR_REF_INTERNA 0          // 0: AVCC (~5 V)   1: referencia interna 1.1 V
                                    // Con 1.1 V la salida del op-amp NO debe superar 1.1 V
                                    // (con shunt 4.7 ohm y Av = 21 -> máx ~11 mA, incluido el offset)// ---------------- Variables globales ----------------
volatile uint32_t ms_contador = 0;
volatile uint8_t  flag_muestreo = 0;

// ---------------- USART ----------------
static void USART_init(void) {
	UCSR0A = (1 << U2X0);                       // doble velocidad
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;
	UCSR0B = (1 << TXEN0);                      // solo transmisión
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);     // 8N1
}

static void USART_transmit(char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

static void USART_send_string(const char *str) {
	while (*str) USART_transmit(*str++);
}

// ---------------- ADC ----------------
static uint16_t ADC_read_once(void) {
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

static void ADC_init(void) {
#if USAR_REF_INTERNA
	ADMUX = (1 << REFS1) | (1 << REFS0);        // 1.1 V interna, canal 0 (A0)
#else
	ADMUX = (1 << REFS0);                       // AVCC, canal 0 (A0)
#endif
	// Prescaler 128 -> 125 kHz
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

	_delay_ms(10);                              // estabilizar la referencia
	(void)ADC_read_once();                      // descartar la primera conversión
}

// Suma de OVERSAMPLE lecturas consecutivas
static uint16_t ADC_read_sum(void) {
	uint16_t suma = 0;
	for (uint8_t i = 0; i < OVERSAMPLE; i++) {
		suma += ADC_read_once();
	}
	return suma;
}

// ---------------- Timer1: tick de 10 ms ----------------
static void Timer1_init(void) {
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);  // CTC, prescaler 64
	OCR1A  = 2499;                                      // 16 MHz / (64 * 2500) = 100 Hz
	TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
	ms_contador += 10;
	flag_muestreo = 1;
}
