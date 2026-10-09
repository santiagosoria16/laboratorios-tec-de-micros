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
