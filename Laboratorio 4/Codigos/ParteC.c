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

// Buffer para comunicación USART (Bluetooth HC-05)
#define BUFFER_SIZE 32
char rx_buffer[BUFFER_SIZE];
volatile uint8_t rx_index = 0;
volatile uint8_t comando_listo = 0;

// Declaración de funciones
void USART_init(unsigned int ubrr);
void USART_transmit_char(char data);
void USART_send_string(const char* str);
void ADC_init(void);
uint16_t ADC_read(uint8_t channel);

// Funciones I2C / TWI
void TWI_init(void);
void TWI_start(void);
void TWI_stop(void);
void TWI_write(uint8_t data);

// Funciones LCD I2C
void lcd_send_nibble(uint8_t nibble, uint8_t rs);
void lcd_send_byte(uint8_t val, uint8_t rs);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_init(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_print(const char* str);
void lcd_clear(void);
void actualizar_lcd(void);

// Lógica de Control
void procesar_comando_bluetooth(void);
void evaluar_sensores(void);
void sonar_sirena(void);
void actualizar_indicadores(void);

// Interrupción por recepción UART (Bluetooth HC-05)
ISR(USART_RX_vect) {
	char ch = UDR0;
	if (ch == '\r' || ch == '\n') {
		if (rx_index > 0) {
			rx_buffer[rx_index] = '\0';
			comando_listo = 1;
			rx_index = 0;
		}
		} else {
		if (rx_index < BUFFER_SIZE - 1) {
			rx_buffer[rx_index++] = ch;
		}
	}
}

int main(void) {
	// 1. Configuración de Entradas / Salidas
	// Puerto B: PB1(Buzzer), PB2(LED Verde), PB3(LED Rojo) como salidas
	DDRB |= (1 << BUZZER_PIN) | (1 << LED_VERDE) | (1 << LED_ROJO);
	PORTB &= ~((1 << BUZZER_PIN) | (1 << LED_VERDE) | (1 << LED_ROJO)); // Apagado inicial

	// Puerto D: PD2 (Botón) como entrada con PULL-UP interno activado
	DDRD &= ~(1 << BOTON_PIN);
	PORTD |= (1 << BOTON_PIN);

	// 2. Inicializar Periféricos
	USART_init(103); // UBRR = 103 para 9600 baudios a F_CPU = 16 MHz
	ADC_init();
	lcd_init();      // Inicializa pantalla LCD vía I2C

	// Habilitar interrupciones globales
	sei();

	// Notificaciones iniciales por UART
	USART_send_string("\r\n--- SISTEMA DE ALARMA INICIADO ---\r\n");
	USART_send_string("MODO: AUTO | ESTADO: DESARMADO\r\n");

	// Pantalla de Bienvenida
	lcd_set_cursor(0, 0);
	lcd_print("  SISTEMA ALARMA ");
	lcd_set_cursor(1, 0);
	lcd_print("  INICIANDOSE... ");
	_delay_ms(1500);
	lcd_clear();

	actualizar_lcd();

	while (1) {
		// Procesar comando entrante desde celular (HC-05)
		if (comando_listo) {
			procesar_comando_bluetooth();
			comando_listo = 0;
		}

		// Evaluar sensores si la alarma está armada
		evaluar_sensores();

		// Actualizar salidas físicas (LEDs y Buzzer)
		actualizar_indicadores();

		// Refrescar LCD solo si hubo un cambio de estado o modo
		if (estado_actual != estado_anterior || modo_actual != modo_anterior) {
			actualizar_lcd();
			estado_anterior = estado_actual;
			modo_anterior = modo_actual;
		}

		_delay_ms(100);
	}

	return 0;
}

// ============================================================================
// DRIVER I2C / TWI
// ============================================================================
void TWI_init(void) {
	TWSR = 0x00; // Prescaler = 1
	TWBR = 72;   // Frecuencia SCL = 100 kHz a 16 MHz
}

void TWI_start(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void TWI_stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void TWI_write(uint8_t data) {
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

// ============================================================================
// DRIVER LCD I2C (PCF8574 en modo 4 bits)
// ============================================================================
void lcd_send_nibble(uint8_t nibble, uint8_t rs) {
	uint8_t data = (nibble & 0xF0) | (rs ? 0x01 : 0x00) | 0x08; // Bit 0x08 = Backlight ON
	TWI_start();
	TWI_write(LCD_ADDR);
	
	// Pulso de Enable
	TWI_write(data | 0x04);
	_delay_us(1);
	TWI_write(data & ~0x04);
	_delay_us(50);
	
	TWI_stop();
}



























