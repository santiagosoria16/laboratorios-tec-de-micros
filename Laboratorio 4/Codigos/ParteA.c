#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define LED_VERDE_PIN   PD6
#define LED_ROJO_PIN    PD7
#define BUZZER_PIN      PB0


#define SPI_SS          PB2
#define SPI_MOSI        PB3
#define SPI_MISO        PB4
#define SPI_SCK         PB5 
#define RC522_RST       PB1 

#define PCF8574_ADDR    (0x27 << 1)

#define LCD_RS          (1 << 0)
#define LCD_EN          (1 << 2)
#define LCD_BACKLIGHT   (1 << 3)

#define EEPROM_MAGIC_ADDR  0x03FF
#define EEPROM_MAGIC_VAL   0xA6

static uint8_t lcd_backlight_val = LCD_BACKLIGHT;

void TWI_Init(void) {
	TWSR = 0x00;
	TWBR = 72; // 100 kHz a 16 MHz
	TWCR = (1 << TWEN);
}

void TWI_Start(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void TWI_Stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void TWI_Write(uint8_t data) {
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void LCD_I2C_WriteNibble(uint8_t data) {
	TWI_Start();
	TWI_Write(PCF8574_ADDR);
	TWI_Write(data | lcd_backlight_val | LCD_EN);
	_delay_us(1);
	TWI_Write((data | lcd_backlight_val) & ~LCD_EN);
	_delay_us(50);
	TWI_Stop();
}

void LCD_I2C_Send(uint8_t value, uint8_t mode) {
	uint8_t highnib = value & 0xF0;
	uint8_t lownib = (value << 4) & 0xF0;
	LCD_I2C_WriteNibble(highnib | mode);
	LCD_I2C_WriteNibble(lownib | mode);
}

void LCD_I2C_Cmd(uint8_t cmd) {
	LCD_I2C_Send(cmd, 0);
}

void LCD_I2C_Data(uint8_t data) {
	LCD_I2C_Send(data, LCD_RS);
}

void LCD_I2C_Init(void) {
	TWI_Init();
	_delay_ms(50);
	
	LCD_I2C_WriteNibble(0x30); _delay_ms(5);
	LCD_I2C_WriteNibble(0x30); _delay_us(150);
	LCD_I2C_WriteNibble(0x30); _delay_ms(1);
	LCD_I2C_WriteNibble(0x20); _delay_ms(1);

	LCD_I2C_Cmd(0x28);
	LCD_I2C_Cmd(0x0C);
	LCD_I2C_Cmd(0x06);
	LCD_I2C_Cmd(0x01);
	_delay_ms(2);
}

void LCD_I2C_SetCursor(uint8_t row, uint8_t col) {
	uint8_t offsets[] = {0x00, 0x40};
	LCD_I2C_Cmd(0x80 | (col + offsets[row]));
}

void LCD_I2C_Print(const char *str) {
	while (*str) {
		LCD_I2C_Data((uint8_t)*str++);
	}
}
void LCD_I2C_Clear(void) {
	LCD_I2C_Cmd(0x01);
	_delay_ms(2);
}


void USART_Init(void) {
	uint16_t ubrr = 103; // 9600 baudios
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

bool Terminal_CheckCard(uint8_t *uid) {
	static char hexBuf[9] = {0};
	static uint8_t bufPos = 0;

	while (UCSR0A & (1 << RXC0)) {
		char c = UDR0;
		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
			hexBuf[bufPos++] = c;
			if (bufPos == 8) {
				for (uint8_t i = 0; i < 4; i++) {
					char byteStr[3] = { hexBuf[i*2], hexBuf[i*2+1], '\0' };
					uid[i] = (uint8_t)strtoul(byteStr, NULL, 16);
				}
				bufPos = 0;
				memset(hexBuf, 0, sizeof(hexBuf));
				return true;
			}
			} else if (c == '\r' || c == '\n') {
			bufPos = 0;
			memset(hexBuf, 0, sizeof(hexBuf));
		}
	}
	return false;
}

