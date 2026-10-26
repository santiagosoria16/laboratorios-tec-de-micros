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

void SPI_Init(void) {
	DDRB |= (1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << SPI_SS) | (1 << RC522_RST);
	DDRB &= ~(1 << SPI_MISO);
	PORTB |= (1 << SPI_SS) | (1 << RC522_RST);
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

uint8_t SPI_Transfer(uint8_t data) {
	SPDR = data;
	while (!(SPSR & (1 << SPIF)));
	return SPDR;
}

void RC522_WriteReg(uint8_t addr, uint8_t val) {
	PORTB &= ~(1 << SPI_SS);
	SPI_Transfer((addr << 1) & 0x7E);
	SPI_Transfer(val);
	PORTB |= (1 << SPI_SS);
}

uint8_t RC522_ReadReg(uint8_t addr) {
	uint8_t val;
	PORTB &= ~(1 << SPI_SS);
	SPI_Transfer(((addr << 1) & 0x7E) | 0x80);
	val = SPI_Transfer(0x00);
	PORTB |= (1 << SPI_SS);
	return val;
}

void RC522_Init(void) {
	PORTB &= ~(1 << RC522_RST);
	_delay_ms(10);
	PORTB |= (1 << RC522_RST);
	_delay_ms(50);

	RC522_WriteReg(0x01, 0x0F);
	_delay_ms(50);

	RC522_WriteReg(0x2A, 0x8D);
	RC522_WriteReg(0x2B, 0x3E);
	RC522_WriteReg(0x2C, 0x00);
	RC522_WriteReg(0x2D, 0x30);
	RC522_WriteReg(0x15, 0x40);
	RC522_WriteReg(0x11, 0x3D);


	RC522_WriteReg(0x26, 0x70);

	uint8_t txControl = RC522_ReadReg(0x14);
	if ((txControl & 0x03) != 0x03) {
		RC522_WriteReg(0x14, txControl | 0x03);
	}
}

bool RC522_RequestA(uint8_t *bufferATQA) {
	RC522_WriteReg(0x0D, 0x07);
	RC522_WriteReg(0x04, 0x7F);
	RC522_WriteReg(0x0A, 0x80);
	RC522_WriteReg(0x01, 0x00);

	RC522_WriteReg(0x09, 0x26);
	RC522_WriteReg(0x01, 0x0C);
	RC522_WriteReg(0x0D, 0x87);

	_delay_ms(20);

	uint8_t irq = RC522_ReadReg(0x04);
	uint8_t error = RC522_ReadReg(0x06);

	if ((irq & 0x20) && !(error & 0x1B)) {
		uint8_t fifoLen = RC522_ReadReg(0x0A) & 0x7F;
		if (fifoLen >= 2) {
			bufferATQA[0] = RC522_ReadReg(0x09);
			bufferATQA[1] = RC522_ReadReg(0x09);
			return true;
		}
	}
	return false;
}

bool RC522_Anticoll(uint8_t *uid) {
	RC522_WriteReg(0x0D, 0x00);
	RC522_WriteReg(0x04, 0x7F);
	RC522_WriteReg(0x0A, 0x80);
	RC522_WriteReg(0x01, 0x00);

	RC522_WriteReg(0x09, 0x93);
	RC522_WriteReg(0x09, 0x20);
	RC522_WriteReg(0x01, 0x0C);
	RC522_WriteReg(0x0D, 0x80);

	_delay_ms(20);

	uint8_t irq = RC522_ReadReg(0x04);
	uint8_t error = RC522_ReadReg(0x06);

	if ((irq & 0x20) && !(error & 0x1B)) {
		uint8_t fifoLen = RC522_ReadReg(0x0A) & 0x7F;
		if (fifoLen >= 4) {
			for (uint8_t i = 0; i < 4; i++) {
				uid[i] = RC522_ReadReg(0x09);
			}
			return true;
		}
	}
	return false;
}

