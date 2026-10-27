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

