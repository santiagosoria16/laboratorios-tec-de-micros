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

void lcd_send_byte(uint8_t val, uint8_t rs) {
	lcd_send_nibble(val & 0xF0, rs);
	lcd_send_nibble((val << 4) & 0xF0, rs);
}

void lcd_send_cmd(uint8_t cmd) {
	lcd_send_byte(cmd, 0);
	_delay_ms(2);
}

void lcd_send_data(uint8_t data) {
	lcd_send_byte(data, 1);
	_delay_us(50);
}

void lcd_init(void) {
	TWI_init();
	_delay_ms(50);

	// Secuencia oficial de inicio HD44780
	lcd_send_nibble(0x30, 0);
	_delay_ms(5);
	lcd_send_nibble(0x30, 0);
	_delay_us(200);
	lcd_send_nibble(0x30, 0);
	_delay_us(200);
	lcd_send_nibble(0x20, 0);
	_delay_ms(5);

	lcd_send_cmd(0x28); // 2 líneas, 5x8 puntos
	lcd_send_cmd(0x0C); // Display ON, Cursor OFF
	lcd_send_cmd(0x06); // Auto increment
	lcd_clear();
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
	uint8_t addr = (row == 0) ? col : (0x40 + col);
	lcd_send_cmd(0x80 | addr);
}

void lcd_print(const char* str) {
	while (*str) {
		lcd_send_data(*str++);
	}
}

void lcd_clear(void) {
	lcd_send_cmd(0x01);
	_delay_ms(2);
}

void actualizar_lcd(void) {
	lcd_clear();
	
	// Línea 1: Modo de Operación
	lcd_set_cursor(0, 0);
	if (modo_actual == MODO_AUTO) {
		lcd_print("MODO: AUTOMATICO");
		} else {
		lcd_print("MODO: MANUAL    ");
	}

	// Línea 2: Estado del Sistema
	lcd_set_cursor(1, 0);
	switch (estado_actual) {
		case ESTADO_DESARMADO:
		lcd_print("EST: DESARMADO  ");
		break;
		case ESTADO_ARMADO:
		lcd_print("EST: ARMADO     ");
		break;
		case ESTADO_ALARMA:
		lcd_print("ALARM: ");
		lcd_print(zona_afectada);
		break;
		case ESTADO_SILENCIADO:
		lcd_print("EST: SILENCIADO ");
		break;
	}
}

// ============================================================================
// PERIFÉRICOS AVR (USART Y ADC)
// ============================================================================
void USART_init(unsigned int ubrr) {
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_transmit_char(char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void USART_send_string(const char* str) {
	while (*str) {
		USART_transmit_char(*str++);
	}
}

void ADC_init(void) {
	ADMUX = (1 << REFS0); // AVcc como referencia
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Prescaler 128
}

uint16_t ADC_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x07);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

// ============================================================================
// LÓGICA DEL SISTEMA Y TONO PARA BUZZER PASIVO
// ============================================================================
void procesar_comando_bluetooth(void) {
	if (strcmp(rx_buffer, "ARM") == 0) {
		estado_actual = ESTADO_ARMADO;
		silenciar_buzzer = 0;
		USART_send_string("ESTADO: ARMADO\r\n");
	}
	else if (strcmp(rx_buffer, "DISARM") == 0) {
		estado_actual = ESTADO_DESARMADO;
		silenciar_buzzer = 0;
		USART_send_string("ESTADO: DESARMADO\r\n");
	}
	else if (strcmp(rx_buffer, "AUTO") == 0) {
		modo_actual = MODO_AUTO;
		USART_send_string("MODO: AUTOMATICO\r\n");
	}
	else if (strcmp(rx_buffer, "MANUAL") == 0) {
		modo_actual = MODO_MANUAL;
		USART_send_string("MODO: MANUAL\r\n");
	}
	else if (strcmp(rx_buffer, "SILENCE") == 0) {
		if (estado_actual == ESTADO_ALARMA) {
			estado_actual = ESTADO_SILENCIADO;
			silenciar_buzzer = 1;
			USART_send_string("ALARMA: SILENCIADA\r\n");
		}
	}
	else if (strcmp(rx_buffer, "FORZAR_ALARMA") == 0) {
		if (modo_actual == MODO_MANUAL) {
			estado_actual = ESTADO_ALARMA;
			silenciar_buzzer = 0;
			strcpy(zona_afectada, "REMOTO");
			USART_send_string("ALARMA:ON; ZONA: FORZADO_REMOTO\r\n");
			} else {
			USART_send_string("ERROR: Solo en MODO MANUAL\r\n");
		}
	}
	else {
		USART_send_string("COMANDO NO RECONOCIDO\r\n");
	}
}

void evaluar_sensores(void) {
	if (estado_actual != ESTADO_ARMADO) {
		return;
	}

	uint16_t ldr_val = ADC_read(0);
	uint8_t boton_presionado = !(PIND & (1 << BOTON_PIN)) ? 1 : 0;

	if (modo_actual == MODO_AUTO) {
		if (ldr_val > LDR_THRESHOLD) {
			estado_actual = ESTADO_ALARMA;
			silenciar_buzzer = 0;
			strcpy(zona_afectada, "VENTANA");
			USART_send_string("ALARMA:ON; ZONA: VENTANA\r\n");
		}
		else if (boton_presionado) {
			estado_actual = ESTADO_ALARMA;
			silenciar_buzzer = 0;
			strcpy(zona_afectada, "MOVIMIENTO");
			USART_send_string("ALARMA:ON; ZONA: MOVIMIENTO_BOTON\r\n");
		}
	}
}

// Función encargada de hacer vibrar la membrana del buzzer pasivo a 2 kHz
void sonar_sirena(void) {
	for (uint8_t i = 0; i < 150; i++) {
		PORTB |= (1 << BUZZER_PIN);
		_delay_us(250); // 250us ALTO + 250us BAJO = 2000 Hz
		PORTB &= ~(1 << BUZZER_PIN);
		_delay_us(250);
	}
}

void actualizar_indicadores(void) {
	switch (estado_actual) {
		case ESTADO_DESARMADO:
		PORTB &= ~(1 << LED_VERDE);
		PORTB &= ~(1 << LED_ROJO);
		PORTB &= ~(1 << BUZZER_PIN);
		break;

		case ESTADO_ARMADO:
		PORTB |= (1 << LED_VERDE);   // LED Verde encendido
		PORTB &= ~(1 << LED_ROJO);
		PORTB &= ~(1 << BUZZER_PIN);
		break;

		case ESTADO_ALARMA:
		PORTB &= ~(1 << LED_VERDE);
		PORTB |= (1 << LED_ROJO);    // LED Rojo encendido

		if (!silenciar_buzzer) {
			sonar_sirena();          // Genera el tono de 2 kHz
			} else {
			PORTB &= ~(1 << BUZZER_PIN);
		}
		break;

		case ESTADO_SILENCIADO:
		PORTB &= ~(1 << LED_VERDE);
		PORTB |= (1 << LED_ROJO);    // Mantiene indicación visual de alerta
		PORTB &= ~(1 << BUZZER_PIN); // Sirena silenciada
		break;
	}
}
