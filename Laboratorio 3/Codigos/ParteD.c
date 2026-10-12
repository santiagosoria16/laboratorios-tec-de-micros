#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

// Dirección I2C del PCF8574 (Cambiar a 0x3F si la pantalla LCD no responde)
#define PCF8574_ADDR 0x27

// ==========================================
// DEFINICIÓN DE FRECUENCIAS (Hz)
// ==========================================
#define SIL 0

// Octava 2
#define B2  123

// Octava 3 - Bajos / Acompañamiento
#define C3  131
#define CS3 139
#define D3  147
#define DS3 156
#define EB3 156
#define E3  165
#define F3  175
#define FS3 185
#define G3  196
#define GS3 208
#define AB3 208
#define A3  220
#define AS3 233
#define BB3 233
#define B3  247

// Octava 4 - Melodía
#define C4  262
#define CS4 277
#define D4  294
#define DS4 311
#define EB4 311
#define E4  330
#define F4  349
#define FS4 370
#define G4  392
#define GS4 415
#define AB4 415
#define A4  440
#define AS4 466
#define BB4 466
#define B4  494

// Octava 5
#define C5  523
#define CS5 554
#define D5  587
#define DS5 622
#define EB5 622
#define E5  659
#define F5  698
#define FS5 740
#define G5  784
#define GS5 831
#define A5  880
#define AS5 932
#define BB5 932
#define B5  988

// Octava 6
#define C6  1047
#define D6  1175

#define FREQ_TO_ICR1(f) ((f > 0) ? ((F_CPU / (8UL * (f))) - 1) : 0)

// ==========================================
// ESTADOS Y MENÚ
// ==========================================
typedef enum {
	ESTADO_MENU,
	ESTADO_PIANO,
	ESTADO_CANCION
} EstadoSistema;

volatile EstadoSistema estadoActual = ESTADO_MENU;
volatile char comandoUART = 0;
volatile uint8_t nuevoComando = 0;

uint8_t opcionMenuActual = 0;
const char* opcionesMenu[] = {
	"1. Piano Manual",
	"2. Never Gonna",
	"3. Free Bird",
	"4. Wake Me Up"
};
#define TOTAL_OPCIONES 4

// ==========================================
// UART
// ==========================================
void UART_Init(unsigned int baud) {
	unsigned int ubrr = F_CPU / 16 / baud - 1;
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;

	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_TxChar(char c) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void UART_SendString(const char *str) {
	while (*str) {
		UART_TxChar(*str++);
	}
}

void UART_MostrarMenu(void) {
	UART_SendString("\r\n==================================================\r\n");
	UART_SendString("    PIANO ELECTRONICO POLIFONICO - ATmega328P     \r\n");
	UART_SendString("==================================================\r\n");
	UART_SendString(" Comandos UART Disponibles:\r\n");
	UART_SendString("  [1] Modo Piano Manual\r\n");
	UART_SendString("  [2] Cancion 1: Never Gonna Give You Up\r\n");
	UART_SendString("  [3] Cancion 2: Free Bird (Solo)\r\n");
	UART_SendString("  [4] Cancion 3: Wake Me Up When September Ends\r\n");
	UART_SendString("  [S] o [0] Detener cancion / Volver al Menu\r\n");
	UART_SendString("  [M] Mostrar este menu interactivo\r\n");
	UART_SendString("==================================================\r\n\r\n");
}

ISR(USART_RX_vect) {
	comandoUART = UDR0;
	nuevoComando = 1;
}

// ==========================================
// I2C CON TIMEOUT
// ==========================================
void I2C_Init(void) {
	TWSR = 0x00;
	TWBR = 0x48; // 100 kHz
	TWCR = (1 << TWEN);
}

uint8_t I2C_Start(void) {
	uint16_t timeout = 1000;
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT))) {
		if (--timeout == 0) return 0;
	}
	return 1;
}

void I2C_Stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	_delay_us(100);
}

uint8_t I2C_Write(uint8_t data) {
	uint16_t timeout = 1000;
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT))) {
		if (--timeout == 0) return 0;
	}
	return 1;
}

// ==========================================
// LCD 16x2 I2C
// ==========================================
#define LCD_BACKLIGHT 0x08

void LCD_I2C_WriteNibble(uint8_t nibble, uint8_t mode) {
	uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;

	if (!I2C_Start()) return;
	if (!I2C_Write(PCF8574_ADDR << 1)) { I2C_Stop(); return; }

	I2C_Write(data | 0x04);
	_delay_us(1);

	I2C_Write(data & ~0x04);
	_delay_us(50);

	I2C_Stop();
}

void LCD_I2C_Cmd(uint8_t cmd) {
	LCD_I2C_WriteNibble(cmd & 0xF0, 0);
	LCD_I2C_WriteNibble((cmd << 4) & 0xF0, 0);

	if (cmd == 0x01 || cmd == 0x02)
	_delay_ms(2);
}

void LCD_I2C_Char(uint8_t data) {
	LCD_I2C_WriteNibble(data & 0xF0, 1);
	LCD_I2C_WriteNibble((data << 4) & 0xF0, 1);
}

void LCD_I2C_Init(void) {
	_delay_ms(50);

	LCD_I2C_WriteNibble(0x30, 0);
	_delay_ms(5);

	LCD_I2C_WriteNibble(0x30, 0);
	_delay_us(200);

	LCD_I2C_WriteNibble(0x30, 0);
	LCD_I2C_WriteNibble(0x20, 0);

	LCD_I2C_Cmd(0x28);
	LCD_I2C_Cmd(0x0C);
	LCD_I2C_Cmd(0x06);
	LCD_I2C_Cmd(0x01);

	_delay_ms(2);
}

void LCD_I2C_SetCursor(uint8_t row, uint8_t col) {
	uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
	LCD_I2C_Cmd(addr);
}

void LCD_I2C_String(const char *str) {
	while (*str)
	LCD_I2C_Char((uint8_t)*str++);
}

void Mostrar_Menu_LCD(void) {
	LCD_I2C_Cmd(0x01);
	LCD_I2C_SetCursor(0, 0);
	LCD_I2C_String("MENU PRINCIPAL:");
	LCD_I2C_SetCursor(1, 0);
	LCD_I2C_String(">");
	LCD_I2C_String(opcionesMenu[opcionMenuActual]);
}

// ==========================================
// PWM / AUDIO
// ==========================================
void PWM_Init(void) {
	// Timer 1 - Melodía - PB1 / OC1A (Fast PWM con ICR1 como TOP)
	TCCR1A = (1 << COM1A1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);

	DDRB |= (1 << PB1);
	PORTB &= ~(1 << PB1);

	// Timer 2 - Bajo - PB3 / OC2A (CTC Mode, Toggle OC2A, Prescaler 256)
	TCCR2A = (1 << WGM21);
	TCCR2B = (1 << CS22) | (1 << CS21);

	DDRB |= (1 << PB3);
	PORTB &= ~(1 << PB3);
}

void Set_Tone_Buzzer1(uint16_t freq) {
	if (freq == 0) {
		TCCR1A &= ~(1 << COM1A1);
		PORTB &= ~(1 << PB1);
		} else {
		TCCR1A |= (1 << COM1A1);
		ICR1 = FREQ_TO_ICR1(freq);
		OCR1A = ICR1 / 2;
	}
}

void Set_Tone_Buzzer2(uint16_t freq) {
	if (freq == 0) {
		TCCR2A &= ~(1 << COM2A0);
		PORTB &= ~(1 << PB3);
		} else {
		TCCR2A |= (1 << COM2A0);
		uint32_t val = ((31250UL + (freq / 2)) / freq) - 1;
		if (val > 255) val = 255;
		OCR2A = (uint8_t)val;
		TCNT2 = 0;
	}
}
