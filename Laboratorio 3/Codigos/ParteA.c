#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#define CALEFACTOR PB0 
#define VENTILADOR PB1    


#define DHT_PORT PORTC
#define DHT_DDR  DDRC
#define DHT_PIN  PINC
#define DHT_BIT  PC0      


#define SDA PC4           
#define SCL PC5      
#define LCD_ADDR 0x27


#define PM_MAX_PERMITIDO 25
#define PM_MIN_PERMITIDO 10

volatile uint8_t flag_medir = 0;
volatile uint16_t contador_ms = 0;

uint8_t temperatura = 0;
uint8_t humedad = 0;

int8_t punto_medio = 20;

void uart_init(void)
{
	uint16_t ubrr = (uint16_t)((F_CPU / (16UL * 9600UL)) - 1);

	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;

	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_putchar(char c)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void uart_print(const char *texto)
{
	while (*texto)
	{
		uart_putchar(*texto++);
	}
}

uint8_t uart_available(void)
{
	return (UCSR0A & (1 << RXC0));
}

char uart_getchar(void)
{
	while (!uart_available());
	return UDR0;
}

void mostrar_menu_uart(void)
{
	uart_print("\r\n========================================\r\n");
	uart_print("       MENU PUNTO MEDIO (PM)\r\n");
	uart_print("========================================\r\n");
	uart_print(" + / u : Incrementar PM (+1 C)\r\n");
	uart_print(" - / d : Decrementar PM (-1 C)\r\n");
	uart_print(" m / M : Mostrar este menu\r\n");
	uart_print("----------------------------------------\r\n");
}

uint8_t dht_wait_level(uint8_t nivel, uint16_t timeout_us)
{
	while ((((DHT_PIN & (1 << DHT_BIT)) != 0) ? 1 : 0) != nivel)
	{
		_delay_us(1);

		if (timeout_us == 0)
		return 0;

		timeout_us--;
	}

	return 1;
}

uint8_t dht11_read(uint8_t *hum, uint8_t *temp)
{
	uint8_t data[5] = {0, 0, 0, 0, 0};
	uint8_t i, j;
	uint16_t ancho;


	cli();

	DHT_DDR |= (1 << DHT_BIT); 
	DHT_PORT &= ~(1 << DHT_BIT);

	_delay_ms(20);


	DHT_PORT |= (1 << DHT_BIT);
	_delay_us(30);

	DHT_DDR &= ~(1 << DHT_BIT);
	DHT_PORT |= (1 << DHT_BIT);



	if (!dht_wait_level(0, 200))
	{
		sei();
		return 1;
	}

	if (!dht_wait_level(1, 200))
	{
		sei();
		return 2;
	}

	if (!dht_wait_level(0, 200))
	{
		sei();
		return 3;
	}

	for (j = 0; j < 5; j++)
	{
		for (i = 0; i < 8; i++)
		{
			if (!dht_wait_level(1, 100))
			{
				sei();
				return 4;
			}
			ancho = 0;

			while (DHT_PIN & (1 << DHT_BIT))
			{
				_delay_us(1);
				ancho++;

				if (ancho >= 100)
				break;
			}

			if (ancho > 40)
			{
				data[j] |= (1 << (7 - i));
			}
		}
	}

	sei();

	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
	{
		return 6;
	}

	*hum = data[0];
	*temp = data[2];

	return 0;
}

void i2c_init(void)
{
	TWSR = 0x00;

	TWBR = (uint8_t)(((F_CPU / 100000UL) - 16) / 2);

	DDRC &= ~((1 << SDA) | (1 << SCL));

	PORTC |= (1 << SDA) | (1 << SCL);
}

uint8_t i2c_start_timeout(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

	uint16_t timer = 10000;

	while (!(TWCR & (1 << TWINT)))
	{
		if (--timer == 0)
		return 1;
	}

	return 0;
}

void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);

	_delay_us(10);
}

void i2c_write_timeout(uint8_t data)
{
	TWDR = data;

	TWCR = (1 << TWINT) | (1 << TWEN);

	uint16_t timer = 10000;

	while (!(TWCR & (1 << TWINT)))
	{
		if (--timer == 0)
		break;
	}
}

void lcd_write(uint8_t data)
{
	if (i2c_start_timeout() == 0)
	{
		i2c_write_timeout(LCD_ADDR << 1);
		i2c_write_timeout(data);
		i2c_stop();
	}
}

void lcd_pulse(uint8_t data)
{
	lcd_write(data | 0x04);

	_delay_us(1);

	lcd_write(data & ~0x04);

	_delay_us(50);
}

void lcd_nibble(uint8_t data, uint8_t rs)
{
	uint8_t salida = data & 0xF0;

	if (rs)
	salida |= 0x01;

	salida |= 0x08;

	lcd_pulse(salida);
}

void lcd_send(uint8_t data, uint8_t rs)
{
	lcd_nibble(data & 0xF0, rs);
	lcd_nibble((data << 4) & 0xF0, rs);
}

void lcd_command(uint8_t cmd)
{
	lcd_send(cmd, 0);
}

void lcd_data(uint8_t data)
{
	lcd_send(data, 1);
}

void lcd_clear(void)
{
	lcd_command(0x01);

	_delay_ms(2);
}

void lcd_goto(uint8_t fila, uint8_t columna)
{
	if (fila == 0)
	lcd_command(0x80 + columna);

	else if (fila == 1)
	lcd_command(0xC0 + columna);
}

void lcd_print_str(const char *str)
{
	while (*str)
	{
		lcd_data(*str++);
	}
}

void lcd_init(void)
{
	_delay_ms(50);

	lcd_nibble(0x30, 0);
	_delay_ms(5);

	lcd_nibble(0x30, 0);
	_delay_us(150);

	lcd_nibble(0x30, 0);

	lcd_nibble(0x20, 0);

	lcd_command(0x28);
	lcd_command(0x0C);

	lcd_clear();
}
