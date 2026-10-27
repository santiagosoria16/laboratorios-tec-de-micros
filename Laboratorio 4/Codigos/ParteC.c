#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// DIRECCIÓN I2C DE LA PANTALLA LCD (Módulo PCF8574)
// Usar 0x4E para dirección de placa 0x27 (0x27 << 1)
// Si la pantalla enciende pero no muestra letras, cambiar 0x4E por 0x7E (0x3F << 1)
// ============================================================================
#define LCD_ADDR 0x4E

// Definición de bits de Puerto B (Salidas)
#define BUZZER_PIN   PB1  // Pin Digital 9 (Señal 'S' del Buzzer)
#define LED_VERDE    PB2  // Pin Digital 10
#define LED_ROJO     PB3  // Pin Digital 11

// Definición de bits de Puerto D (Entradas)
#define BOTON_PIN    PD2  // Pin Digital 2 (Botón de presencia)

// Umbral analógico para el sensor LDR (Apertura de ventana/portón)
#define LDR_THRESHOLD 500

// Estados de la Máquina de Estados
typedef enum {
	ESTADO_DESARMADO = 0,
	ESTADO_ARMADO,
	ESTADO_ALARMA,
	ESTADO_SILENCIADO
} EstadoAlarma;

// Modos de Operación
typedef enum {
	MODO_AUTO = 0,
	MODO_MANUAL
} ModoOperacion;

// Variables Globales del Sistema
volatile EstadoAlarma estado_actual = ESTADO_DESARMADO;
volatile EstadoAlarma estado_anterior = 0xFF; // Refresco inteligente del LCD
volatile ModoOperacion modo_actual = MODO_AUTO;
volatile ModoOperacion modo_anterior = 0xFF;
volatile uint8_t silenciar_buzzer = 0;
char zona_afectada[16] = "";
