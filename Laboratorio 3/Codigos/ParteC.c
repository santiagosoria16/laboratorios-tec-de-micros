
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

void lcd_set_cursor(uint8_t col, uint8_t row)
{
	uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
	lcd_command(addr);
}

void lcd_print(const char *str)
{
	while (*str)
	lcd_char(*str++);
}

#define RGB_PORT PORTB
#define RGB_DDR  DDRB

#define LED_R PB2
#define LED_G PB3
#define LED_B PB4

#define SERVO_DDR DDRB
#define SERVO_PIN PB1

typedef struct {
	const char *nombre;
	uint16_t r_ref;
	uint16_t g_ref;
	uint16_t b_ref;
	uint8_t angulo_servo;
	uint8_t r_out;
	uint8_t g_out;
	uint8_t b_out;
} PatronColor;

#define NUM_COLORES 5

const PatronColor BANCO_COLORES[NUM_COLORES] = {
	{"ROJO",     720, 330, 280,  30, 1, 0, 0},
	{"VERDE",    120, 470, 130,  70, 0, 1, 0},
	{"AZUL",     260, 350, 539, 110, 0, 0, 1},
	{"VIOLETA",  410, 180, 235, 130, 1, 0, 1},
	{"AMARILLO", 690, 660, 276, 150, 1, 1, 0}
};

void rgb_init(void)
{
	RGB_DDR |= (1 << LED_R) | (1 << LED_G) | (1 << LED_B);

	RGB_PORT &= ~((1 << LED_R) |
	(1 << LED_G) |
	(1 << LED_B));
}

void set_rgb_color(uint8_t r, uint8_t g, uint8_t b)
{
	if (r)
	RGB_PORT |= (1 << LED_R);
	else
	RGB_PORT &= ~(1 << LED_R);

	if (g)
	RGB_PORT |= (1 << LED_G);
	else
	RGB_PORT &= ~(1 << LED_G);

	if (b)
	RGB_PORT |= (1 << LED_B);
	else
	RGB_PORT &= ~(1 << LED_B);
}

void adc_init(void)
{
	ADMUX = (1 << REFS0);

	ADCSRA = (1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);
}

uint16_t adc_read(uint8_t canal)
{
	ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);

	ADCSRA |= (1 << ADSC);

	uint32_t timeout = 60000UL;

	while (ADCSRA & (1 << ADSC)) {
		if (--timeout == 0)
		return ADC;
	}

	return ADC;
}

uint16_t adc_read_promedio(uint8_t canal)
{
	adc_read(canal);
	_delay_ms(2);

	uint32_t suma = 0;

	for (uint8_t i = 0; i < 8; i++) {
		suma += adc_read(canal);
		_delay_ms(2);
	}

	return (uint16_t)(suma / 8);
}

void uart_init(uint32_t baud)
{
	uint16_t ubrr = (F_CPU / (16UL * baud)) - 1;

	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;

	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_print(const char *str)
{
	while (*str) {
		while (!(UCSR0A & (1 << UDRE0))) {
		}

		UDR0 = *str++;
	}
}

void servo_init(void)
{
	SERVO_DDR |= (1 << SERVO_PIN);

	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;

	ICR1 = 39999;
	OCR1A = 3000;

	TCCR1A = (1 << COM1A1) | (1 << WGM11);

	TCCR1B = (1 << WGM13) |
	(1 << WGM12) |
	(1 << CS11);
}

void servo_set_angle(uint8_t angulo)
{
	if (angulo > 180)
	angulo = 180;

	uint16_t pulso =
	2000 + ((uint32_t)angulo * 2000UL) / 180UL;

	OCR1A = pulso;
}

uint16_t integer_sqrt(uint32_t n)
{
	uint32_t root = 0;
	uint32_t bit = 1UL << 30;

	while (bit > n)
	bit >>= 2;

	while (bit != 0) {
		if (n >= root + bit) {
			n -= root + bit;
			root = (root >> 1) + bit;
			} else {
			root >>= 1;
		}

		bit >>= 2;
	}

	return (uint16_t)root;
}

typedef enum {
	ESTADO_MEDICION_ROJO,
	ESTADO_MEDICION_VERDE,
	ESTADO_MEDICION_AZUL,
	ESTADO_PROCESAMIENTO,
	ESTADO_ACTUACION,
	ESTADO_ESPERA
} EstadoSistema;

int main(void)
{
	rgb_init();
	adc_init();
	uart_init(9600);
	servo_init();
	lcd_init();

	lcd_set_cursor(0, 0);
	lcd_print("CLASIFICADOR DE");

	lcd_set_cursor(0, 1);
	lcd_print("COLOR PLASTICOS");

	_delay_ms(1500);
	lcd_command(0x01);

	EstadoSistema estado = ESTADO_MEDICION_ROJO;

	uint16_t r_med = 0;
	uint16_t g_med = 0;
	uint16_t b_med = 0;

	uint8_t idx_detectado = 0;
	uint16_t dist_minima = 0;

	char buffer[64];

	while (1) {
		switch (estado) {

			
			case ESTADO_MEDICION_ROJO:
			set_rgb_color(1, 0, 0);
			_delay_ms(150);

			r_med = adc_read_promedio(0);

			estado = ESTADO_MEDICION_VERDE;
			break;

			case ESTADO_MEDICION_VERDE:
			set_rgb_color(0, 1, 0);
			_delay_ms(150);

			g_med = adc_read_promedio(0);

			estado = ESTADO_MEDICION_AZUL;
			break;

			case ESTADO_MEDICION_AZUL:
			set_rgb_color(0, 0, 1);
			_delay_ms(150);

			b_med = adc_read_promedio(0);

			estado = ESTADO_PROCESAMIENTO;
			break;


			case ESTADO_PROCESAMIENTO: {
				uint32_t dist_minima_sq = 0xFFFFFFFFUL;
				idx_detectado = 0;

				for (uint8_t i = 0; i < NUM_COLORES; i++) {
					int32_t dr =
					(int32_t)r_med - BANCO_COLORES[i].r_ref;

					int32_t dg =
					(int32_t)g_med - BANCO_COLORES[i].g_ref;

					int32_t db =
					(int32_t)b_med - BANCO_COLORES[i].b_ref;

					uint32_t dist_sq =
					(uint32_t)(dr * dr + dg * dg + db * db);

					if (dist_sq < dist_minima_sq) {
						dist_minima_sq = dist_sq;
						idx_detectado = i;
					}
				}
