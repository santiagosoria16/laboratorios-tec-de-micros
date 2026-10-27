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



















