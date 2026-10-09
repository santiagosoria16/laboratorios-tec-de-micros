
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>
#define LCD_I2C_ADDR  (0x27 << 1)
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

uint8_t twi_wait(void)
{
	uint32_t timeout = 60000UL;

	while (!(TWCR & (1 << TWINT))) {
		if (--timeout == 0)
		return 0;
	}

	return 1;
}

void twi_init(void)
{
	TWSR = 0x00;
	TWBR = 72;
	TWCR = (1 << TWEN);
}

void twi_start(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	twi_wait();
}

void twi_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	_delay_us(100);
}

void twi_write(uint8_t data)
{
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	twi_wait();
}

void lcd_i2c_write_byte(uint8_t data)
{
	twi_start();
	twi_write(LCD_I2C_ADDR);
	twi_write(data | LCD_BACKLIGHT);
	twi_stop();
}

void lcd_send_nibble(uint8_t nibble, uint8_t mode)
{
	uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;

	lcd_i2c_write_byte(data);
	lcd_i2c_write_byte(data | LCD_ENABLE);
	_delay_us(1);
	lcd_i2c_write_byte(data & ~LCD_ENABLE);
	_delay_us(50);
}

void lcd_command(uint8_t cmd)
{
	lcd_send_nibble(cmd & 0xF0, 0);
	lcd_send_nibble((cmd << 4) & 0xF0, 0);
	_delay_ms(2);
}

void lcd_char(uint8_t data)
{
	lcd_send_nibble(data & 0xF0, LCD_RS);
	lcd_send_nibble((data << 4) & 0xF0, LCD_RS);
	_delay_us(100);
}

void lcd_init(void)
{
	twi_init();
	_delay_ms(50);

	lcd_send_nibble(0x30, 0);
	_delay_ms(5);
	lcd_send_nibble(0x30, 0);
	_delay_us(150);
	lcd_send_nibble(0x30, 0);
	lcd_send_nibble(0x20, 0);

	lcd_command(0x28);
	lcd_command(0x0C);
	lcd_command(0x01);
	_delay_ms(2);
}
