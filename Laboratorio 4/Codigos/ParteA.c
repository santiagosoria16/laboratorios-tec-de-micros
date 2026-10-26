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

