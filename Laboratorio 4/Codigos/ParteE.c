#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define HOLD_PIN   PD2
#define CALIB_PIN  PD3
#define BUZZER_PIN PD4
#define LED_PIN    PD6

#define MPU6050_ADDR 0xD0
#define LCD_ADDR     0x4E

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

#define EEPROM_MAGIC 0xA5
uint8_t EEMEM eeprom_flag;
float EEMEM eeprom_offset_x;
float EEMEM eeprom_offset_y;

float offset_x = 0.0f;
float offset_y = 0.0f;

uint8_t led_buffer[192];

void twi_init(void) {
	TWSR = 0x00; 
	TWBR = 72;  
	TWCR = (1 << TWEN);
}

void twi_start(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void twi_stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	_delay_us(100);
}

void twi_write(uint8_t data) {
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

uint8_t twi_read_ack(void) {
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

uint8_t twi_read_nack(void) {
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

void mpu6050_init(void) {
	twi_start();
	twi_write(MPU6050_ADDR);
	twi_write(0x6B);
	twi_write(0x00);
	twi_stop();
}

void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
	twi_start();
	twi_write(MPU6050_ADDR);
	twi_write(0x3B);
	twi_start();
	twi_write(MPU6050_ADDR | 0x01); \
	
	uint8_t xh = twi_read_ack();
	uint8_t xl = twi_read_ack();
	uint8_t yh = twi_read_ack();
	uint8_t yl = twi_read_ack();
	uint8_t zh = twi_read_ack();
	uint8_t zl = twi_read_nack();
	twi_stop();

	*ax = (int16_t)((xh << 8) | xl);
	*ay = (int16_t)((yh << 8) | yl);
	*az = (int16_t)((zh << 8) | zl);
}

void lcd_send_nibble(uint8_t nibble, uint8_t mode) {
	uint8_t data = nibble | mode | LCD_BACKLIGHT;
	twi_start();
	twi_write(LCD_ADDR);
	twi_write(data | LCD_ENABLE);
	_delay_us(1);
	twi_write(data & ~LCD_ENABLE);
	_delay_us(50);
	twi_stop();
}

void lcd_send_byte(uint8_t val, uint8_t mode) {
	lcd_send_nibble(val & 0xF0, mode);
	lcd_send_nibble((val << 4) & 0xF0, mode);
}

void lcd_command(uint8_t cmd) { lcd_send_byte(cmd, 0); }
void lcd_char(uint8_t data)   { lcd_send_byte(data, LCD_RS); }

void lcd_init(void) {
	_delay_ms(50);
	lcd_send_nibble(0x30, 0); _delay_ms(5);
	lcd_send_nibble(0x30, 0); _delay_us(150);
	lcd_send_nibble(0x30, 0);
	lcd_send_nibble(0x20, 0); // Modo 4 bits

	lcd_command(0x28);
	lcd_command(0x0C);
	lcd_command(0x06);
	lcd_command(0x01);
	_delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
	uint8_t offsets[] = {0x00, 0x40};
	lcd_command(0x80 | (col + offsets[row]));
}

void lcd_print(const char *str) {
	while (*str) lcd_char(*str++);
}


void ws2812_show(void) {
	uint8_t masklo = PORTD & ~(1 << LED_PIN);
	uint8_t maskhi = PORTD | (1 << LED_PIN);
	
	cli();
	for (uint16_t i = 0; i < 192; i++) {
		uint8_t b = led_buffer[i];
		for (uint8_t bit = 0; bit < 8; bit++) {
			if (b & 0x80) {
				PORTD = maskhi;
				asm volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t");
				PORTD = masklo;
				asm volatile ("nop\n\tnop\n\tnop\n\t");
				} else {
				PORTD = maskhi;
				asm volatile ("nop\n\tnop\n\tnop\n\t");
				PORTD = masklo;
				asm volatile ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t");
			}
			b <<= 1;
		}
	}
	sei();
	_delay_us(50);
}

void set_pixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
	uint16_t index = ((row * 8) + col) * 3;
	led_buffer[index]     = g;
	led_buffer[index + 1] = r;
	led_buffer[index + 2] = b;
}

void clear_pixels(void) {
	for (uint16_t i = 0; i < 192; i++) led_buffer[i] = 0;
}

