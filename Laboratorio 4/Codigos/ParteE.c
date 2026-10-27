#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

// Definición de pines en Puerto D
#define HOLD_PIN   PD2
#define CALIB_PIN  PD3
#define BUZZER_PIN PD4
#define LED_PIN    PD6

// MPU6050 y LCD I2C
#define MPU6050_ADDR 0xD0 // (0x68 << 1)
#define LCD_ADDR     0x4E // (0x27 << 1)

// Control LCD I2C (PCF8574)
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

// EEPROM
#define EEPROM_MAGIC 0xA5
uint8_t EEMEM eeprom_flag;
float EEMEM eeprom_offset_x;
float EEMEM eeprom_offset_y;

float offset_x = 0.0f;
float offset_y = 0.0f;

// Buffer para 64 LEDs RGB (192 bytes en formato G, R, B)
uint8_t led_buffer[192];

// ==================== DRIVER TWI (I2C) ====================
void twi_init(void) {
	TWSR = 0x00; // Prescaler = 1
	TWBR = 72;   // 100 kHz SCL @ 16 MHz
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

// ==================== DRIVER MPU6050 ====================
void mpu6050_init(void) {
	twi_start();
	twi_write(MPU6050_ADDR);
	twi_write(0x6B); // Registro PWR_MGMT_1
	twi_write(0x00); // Despertar sensor
	twi_stop();
}

void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
	twi_start();
	twi_write(MPU6050_ADDR);
	twi_write(0x3B); // ACCEL_XOUT_H
	twi_start();
	twi_write(MPU6050_ADDR | 0x01); // Modo lectura
	
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

// ==================== DRIVER LCD 16x2 I2C ====================
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

	lcd_command(0x28); // 2 líneas, fuente 5x8
	lcd_command(0x0C); // Enceder pantalla, ocultar cursor
	lcd_command(0x06); // Mover cursor a la derecha
	lcd_command(0x01); // Limpiar pantalla
	_delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
	uint8_t offsets[] = {0x00, 0x40};
	lcd_command(0x80 | (col + offsets[row]));
}

void lcd_print(const char *str) {
	while (*str) lcd_char(*str++);
}

// ==================== DRIVER WS2812B (800 kHz) ====================
void ws2812_show(void) {
	uint8_t masklo = PORTD & ~(1 << LED_PIN);
	uint8_t maskhi = PORTD | (1 << LED_PIN);
	
	cli(); // Desactivar interrupciones para respetar la temporización exacta
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

// ==================== GENERADOR DE TONO BUZZER ====================
void buzzer_sound(bool state) {
	if (state) {
		// Genera tono de ~2 kHz compatible con buzzers activos y pasivos
		for (uint8_t i = 0; i < 30; i++) {
			PORTD |= (1 << BUZZER_PIN);
			_delay_us(250);
			PORTD &= ~(1 << BUZZER_PIN);
			_delay_us(250);
		}
		} else {
		PORTD &= ~(1 << BUZZER_PIN);
	}
}

// ==================== GESTIÓN DE EEPROM ====================
void load_eeprom(void) {
	if (eeprom_read_byte(&eeprom_flag) == EEPROM_MAGIC) {
		offset_x = eeprom_read_float(&eeprom_offset_x);
		offset_y = eeprom_read_float(&eeprom_offset_y);
		} else {
		offset_x = 0.0f;
		offset_y = 0.0f;
	}
}

void save_eeprom(float ox, float oy) {
	eeprom_update_float(&eeprom_offset_x, ox);
	eeprom_update_float(&eeprom_offset_y, oy);
	eeprom_update_byte(&eeprom_flag, EEPROM_MAGIC);
	offset_x = ox;
	offset_y = oy;
}

// ==================== PROGRAMA PRINCIPAL ====================
int main(void) {
	// Configuración de puertos
	DDRD |= (1 << LED_PIN) | (1 << BUZZER_PIN);    // Entradas/Salidas en Puerto D
	DDRD &= ~((1 << HOLD_PIN) | (1 << CALIB_PIN));
	PORTD |= (1 << HOLD_PIN) | (1 << CALIB_PIN);   // Habilitar Pull-ups internos

	twi_init();
	mpu6050_init();
	lcd_init();

	lcd_set_cursor(0, 0);
	lcd_print("Burbuja Nivel");
	lcd_set_cursor(0, 1);
	lcd_print("Iniciando...");
	_delay_ms(1000);
	lcd_command(0x01);

	load_eeprom();

	float filt_pitch = 0.0f, filt_roll = 0.0f;
	bool hold = false;
	uint8_t last_hold = 1, last_calib = 1;

	while (1) {
		uint8_t read_hold = (PIND & (1 << HOLD_PIN)) ? 1 : 0;
		uint8_t read_calib = (PIND & (1 << CALIB_PIN)) ? 1 : 0;

		// Anti-rebote y detección de pulsación HOLD
		if (last_hold == 1 && read_hold == 0) {
			hold = !hold;
			_delay_ms(50);
		}
		last_hold = read_hold;

		// Lectura de acelerómetro
		int16_t ax, ay, az;
		mpu6050_read_accel(&ax, &ay, &az);

		float fax = (float)ax / 16384.0f;
		float fay = (float)ay / 16384.0f;
		float faz = (float)az / 16384.0f;

		float raw_pitch = atan2(fay, sqrt(fax * fax + faz * faz)) * 180.0f / M_PI;
		float raw_roll  = atan2(-fax, sqrt(fay * fay + faz * faz)) * 180.0f / M_PI;

		// Pulsación CALIBRAR
		if (last_calib == 1 && read_calib == 0) {
			save_eeprom(raw_pitch, raw_roll);
			_delay_ms(50);
		}
		last_calib = read_calib;

		if (!hold) {
			float cal_pitch = raw_pitch - offset_x;
			float cal_roll  = raw_roll  - offset_y;

			// Filtro complementario / Media móvil
			filt_pitch = (0.8f * filt_pitch) + (0.2f * cal_pitch);
			filt_roll  = (0.8f * filt_roll)  + (0.2f * cal_roll);
		}

		float abs_p = fabs(filt_pitch);
		float abs_r = fabs(filt_roll);

		uint8_t row = 3, col = 3;
		uint8_t r = 0, g = 0, b = 0;

		// Zona muerta (< +-3 grados)
		if (abs_p < 3.0f && abs_r < 3.0f) {
			row = 3; col = 3;
			b = 60; // Azul (Centrado)
			} else {
			// Mapeo [-30, 30] a rango [0, 7]
			float p_clamped = filt_pitch < -30.0f ? -30.0f : (filt_pitch > 30.0f ? 30.0f : filt_pitch);
			float r_clamped = filt_roll  < -30.0f ? -30.0f : (filt_roll  > 30.0f ? 30.0f : filt_roll);

			col = (uint8_t)((r_clamped + 30.0f) * 7.0f / 60.0f);
			row = (uint8_t)((30.0f - p_clamped) * 7.0f / 60.0f);

			if (abs_r > 2.0f * abs_p) {
				r = 60; // Rojo (Predomina X)
				} else if (abs_p > 2.0f * abs_r) {
				g = 60; // Verde (Predomina Y)
				} else {
				r = 60; g = 60; // Amarillo (Combinado)
			}
		}

		// Alerta sonora si toca bordes de la matriz
		bool border = (row == 0 || row == 7 || col == 0 || col == 7);
		buzzer_sound(border && !hold);

		clear_pixels();
		set_pixel(row, col, r, g, b);
		ws2812_show();

		// Actualización de LCD
		char buffer[17];
		lcd_set_cursor(0, 0);
		snprintf(buffer, sizeof(buffer), "P:%3d%c R:%3d%c   ", (int)filt_pitch, 0xDF, (int)filt_roll, 0xDF);
		lcd_print(buffer);

		lcd_set_cursor(0, 1);
		if (hold) {
			lcd_print("MODO: HOLD     ");
			} else {
			lcd_print("MODO: NORMAL   ");
		}

		_delay_ms(35);
	}
}
