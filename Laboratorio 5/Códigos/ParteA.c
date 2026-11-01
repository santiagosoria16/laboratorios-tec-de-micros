#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>

// --- DEFINICIÓN DE PINES ---
#define LED_PIN       PB5   // Pin 13 Arduino
#define BUZZER_PIN    PB0   // Pin 8 Arduino
#define BUTTON_PIN    PD2   // Pin 2 Arduino (INT0)

#define LCD_RS_PIN    PB1   // Pin 9 Arduino
#define LCD_E_PIN     PB2   // Pin 10 Arduino

// --- TIPOS DE DATOS Y FSM ---
typedef enum {
	ESTADO_ACTIVO = 0,
	ESTADO_STANDBY,
	ESTADO_APAGADO
} EstadoSistema;

typedef void (*FuncionEstado)(void);

// --- VARIABLES GLOBALES VOLÁTILES ---
volatile uint32_t milisegundos_totales = 0;
volatile uint32_t ultimo_evento_usuario = 0;

volatile uint16_t cont_led_ms = 0;
volatile uint16_t cont_lcd_ms = 0;

volatile bool flag_tarea_led = false;
volatile bool flag_tarea_lcd = false;
volatile bool flag_evento_boton = false;

EstadoSistema id_estado_actual = ESTADO_ACTIVO;


// --- DRIVER LCD ROBUSTO PARA PICSIMLAB ---

void lcd_send_nibble(uint8_t nibble) {
	// Escribe únicamente en PD4, PD5, PD6 y PD7 manteniendo el resto del puerto
	PORTD = (PORTD & 0x0F) | (nibble & 0xF0);
	
	// Pulso de Enable amplio para asegurar captura en el simulador
	PORTB |= (1 << LCD_E_PIN);
	_delay_ms(2);
	PORTB &= ~(1 << LCD_E_PIN);
	_delay_ms(2);
}

void lcd_send_byte(uint8_t byte, uint8_t es_dato) {
	if (es_dato) {
		PORTB |= (1 << LCD_RS_PIN);  // RS = 1 (Texto)
		} else {
		PORTB &= ~(1 << LCD_RS_PIN); // RS = 0 (Comando)
	}

	lcd_send_nibble(byte & 0xF0);        // Parte alta
	lcd_send_nibble((byte << 4) & 0xF0); // Parte baja
}

void LCD_Init(void) {
	// Configurar PD4..PD7, PB1 y PB2 como salidas
	DDRD |= 0xF0;
	DDRB |= (1 << LCD_RS_PIN) | (1 << LCD_E_PIN);

	PORTB &= ~((1 << LCD_RS_PIN) | (1 << LCD_E_PIN));
	_delay_ms(100); // Espera inicial de encendido

	// Secuencia estándar HD44780
	lcd_send_nibble(0x30); _delay_ms(10);
	lcd_send_nibble(0x30); _delay_ms(5);
	lcd_send_nibble(0x30); _delay_ms(5);
	lcd_send_nibble(0x20); _delay_ms(5); // Activa bus de 4 bits

	lcd_send_byte(0x28, 0); // 2 líneas, modo 5x8
	lcd_send_byte(0x0C, 0); // Encender pantalla, sin cursor
	lcd_send_byte(0x01, 0); // Limpiar pantalla
	_delay_ms(10);
	lcd_send_byte(0x06, 0); // Incremento automático
}

void LCD_SetCursor(uint8_t fila, uint8_t columna) {
	uint8_t addr = (fila == 0) ? (0x80 + columna) : (0xC0 + columna);
	lcd_send_byte(addr, 0);
}

void LCD_Print(const char* str) {
	while (*str) {
		lcd_send_byte(*str++, 1);
	}
}

void LCD_Clear(void) {
	lcd_send_byte(0x01, 0);
	_delay_ms(10);
}

// --- CONFIGURACIÓN DE TIMERS E INTERRUPCIONES ---

void Timer0_Init(void) {
	TCCR0A = (1 << WGM01);              // CTC modo 2
	TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler 64
	OCR0A = 249;                        // 1 ms exacto
	TIMSK0 |= (1 << OCIE0A);
}

void INT0_Init(void) {
	EICRA |= (1 << ISC01);  // Interrupción por flanco de bajada
	EIMSK |= (1 << INT0);
}
