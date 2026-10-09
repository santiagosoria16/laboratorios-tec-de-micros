
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

