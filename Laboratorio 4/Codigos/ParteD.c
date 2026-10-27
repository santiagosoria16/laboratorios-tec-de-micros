#ifndef F_CPU
#define F_CPU 16000000UL // Frecuencia de reloj de 16 MHz
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// Definición de Pines de Hardware
// -----------------------------------------------------------------------------
#define WS2812_PORT PORTB
#define WS2812_DDR  DDRB
#define WS2812_PIN  PB0      // Pin 8 (PB0) para la Matriz LED RGB WS2812B

#define BUZZER_PORT PORTD
#define BUZZER_DDR  DDRD
#define BUZZER_PIN  PD3      // Pin 3 (PD3 / OC2B) para el Buzzer

#define JOY_SW_PORT PORTD
#define JOY_SW_PINR PIND
#define JOY_SW_DDR  DDRD
#define JOY_SW_BIT  PD2      // Pin 2 (PD2) para el Pulsador SW del Joystick

// Intercambio de canales ADC para corregir la orientación del joystick:
#define ADC_JOY_X   1        // Canal ADC1 (PC1 / A1) para Eje X del Joystick
#define ADC_JOY_Y   0        // Canal ADC0 (PC0 / A0) para Eje Y del Joystick

// -----------------------------------------------------------------------------
// Constantes del Sistema
// -----------------------------------------------------------------------------
#define EEPROM_ADDR_RECORD ((uint8_t*)0x00)
#define NUM_LEDS           64
#define MAX_SECUENCIA      50
#define UMBRAL_ADC_BAJO    350
#define UMBRAL_ADC_ALTO    670

// Frecuencias de tonos para los 4 colores (Do, Mi, Sol, Si)
const uint16_t TONOS[4] = {262, 330, 392, 494};

// Estructura de Color RGB
typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} ColorRGB;

// Paleta de Colores
const ColorRGB COLORES[4] = {
	{255, 0,   0  }, // 0: Rojo
	{0,   255, 0  }, // 1: Verde
	{0,   0,   255}, // 2: Azul
	{255, 255, 0  }  // 3: Amarillo
};

// Estructura de Paso de la Secuencia
typedef struct {
	uint8_t x;
	uint8_t y;
	uint8_t colorIdx;
} PasoSecuencia;

// Estados de la Máquina de Estados Finitos (FSM)
typedef enum {
	ESTADO_INICIO_LIBRE,
	ESTADO_GENERAR_PASO,
	ESTADO_REPRODUCIR_SECUENCIA,
	ESTADO_TURNO_JUGADOR,
	ESTADO_EXITO,
	ESTADO_ERROR
} EstadoJuego;

// Matriz de píxeles (GRB para WS2812B)
ColorRGB matriz[NUM_LEDS];

// -----------------------------------------------------------------------------
// Controlador WS2812B (Protocolo de Tiempo mediante Ensamblador Inline)
// -----------------------------------------------------------------------------
void ws2812_mostrar(void) {
	cli(); // Deshabilitar interrupciones para mantener sincronización estricta
	for (uint16_t i = 0; i < NUM_LEDS; i++) {
		// WS2812B requiere secuencia de envío: Verde, Rojo, Azul
		uint8_t datos[3] = {matriz[i].g, matriz[i].r, matriz[i].b};
		
		for (uint8_t j = 0; j < 3; j++) {
			uint8_t byte = datos[j];
			for (int8_t bit = 7; bit >= 0; bit--) {
				if (byte & (1 << bit)) {
					// Bit '1': T1H ~700ns, T1L ~600ns
					WS2812_PORT |= (1 << WS2812_PIN);
					asm volatile("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t");
					WS2812_PORT &= ~(1 << WS2812_PIN);
					asm volatile("nop\n\tnop\n\tnop\n\tnop\n\t");
					} else {
					// Bit '0': T0H ~350ns, T0L ~800ns
					WS2812_PORT |= (1 << WS2812_PIN);
					asm volatile("nop\n\tnop\n\tnop\n\tnop\n\t");
					WS2812_PORT &= ~(1 << WS2812_PIN);
					asm volatile("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t");
				}
			}
		}
	}
	sei(); // Habilitar interrupciones
	_delay_us(60); // Reset latch de WS2812B (>50us)
}

void limpiar_matriz(void) {
	for (uint16_t i = 0; i < NUM_LEDS; i++) {
		matriz[i].r = 0;
		matriz[i].g = 0;
		matriz[i].b = 0;
	}
	ws2812_mostrar();
}

uint8_t xy_a_index(uint8_t x, uint8_t y) {
	return (y * 8) + x;
}

// -----------------------------------------------------------------------------
// Generación de Sonido con Timer2 (Modo CTC)
// -----------------------------------------------------------------------------
void buzzer_tono(uint16_t freq_hz) {
	if (freq_hz == 0) {
		TCCR2A = 0;
		TCCR2B = 0;
		BUZZER_PORT &= ~(1 << BUZZER_PIN);
		return;
	}
	// Prescaler = 256. F_CPU = 16MHz.
	uint32_t ocr_val = (F_CPU / (2UL * 256UL * freq_hz)) - 1;
	if (ocr_val > 255) ocr_val = 255;

	OCR2A = (uint8_t)ocr_val;
	TCCR2A = (1 << WGM21) | (1 << COM2B0); // Modo CTC, conmuta pin OC2B (PD3)
	TCCR2B = (1 << CS22) | (1 << CS21);   // Prescaler 256
}

void emitir_tono(uint16_t freq_hz, uint16_t duracion_ms) {
	buzzer_tono(freq_hz);
	for (uint16_t i = 0; i < duracion_ms; i++) {
		_delay_ms(1);
	}
	buzzer_tono(0);
}

// -----------------------------------------------------------------------------
// Driver ADC (Convertidor Analógico a Digital)
// -----------------------------------------------------------------------------
void adc_init(void) {
	// Referencia AVcc con condensador externo en AREF
	ADMUX = (1 << REFS0);
	// Habilitar ADC y prescaler de 128 (16MHz / 128 = 125kHz de reloj ADC)
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_leer(uint8_t canal) {
	ADMUX = (ADMUX & 0xF0) | (canal & 0x07);
	ADCSRA |= (1 << ADSC); // Iniciar conversión
	while (ADCSRA & (1 << ADSC)); // Esperar fin de conversión
	return ADC;
}
















