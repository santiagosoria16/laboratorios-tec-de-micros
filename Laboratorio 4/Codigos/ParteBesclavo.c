#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#define CMD_START       0x01
#define CMD_STOP        0x02
#define CMD_SPEED       0x03
#define CMD_PISTON      0x04
#define CMD_EMERGENCY   0x05

#define LED_VERDE       PB0
#define LED_ROJO        PC0
#define BUZZER          PC1

#define MOTOR_IN1       PD5
#define MOTOR_EN        PD6
#define MOTOR_IN2       PD7

#define SERVO_PIN       PB1

#define SPI_SS          PB2
#define SPI_MOSI        PB3
#define SPI_MISO        PB4
#define SPI_SCK         PB5

typedef enum
{
	RECIBIR_COMANDO = 0,
	RECIBIR_VALOR
} estado_spi_t;

typedef enum
{
	EST_PARADO = 0,
	EST_MARCHA,
	EST_PISTON,
	EST_EMERGENCIA
} estado_esclavo_t;

volatile estado_spi_t fase_spi = RECIBIR_COMANDO;

volatile estado_esclavo_t estado = EST_PARADO;

volatile uint8_t comando_recibido = 0;
volatile uint8_t valor_recibido = 0;

volatile uint16_t tiempo_servo = 0;

void motor_detener(void)
{
	TCCR0A = 0;

	OCR0A = 0;

	PORTD &= ~((1 << MOTOR_IN1) |
	(1 << MOTOR_IN2) |
	(1 << MOTOR_EN));
}

void motor_velocidad(uint8_t velocidad)
{
	if (velocidad == 0)
	{
		TCCR0A = 0;

		OCR0A = 0;

		PORTD &= ~(1 << MOTOR_EN);
	}
	else
	{
		PORTD |= (1 << MOTOR_IN1);

		PORTD &= ~(1 << MOTOR_IN2);

		TCCR0A = (1 << COM0A1) |
		(1 << WGM01) |
		(1 << WGM00);

		OCR0A = velocidad;
	}
}

void timer0_init(void)
{
	TCCR0A = 0;

	TCCR0B = (1 << CS01) |
	(1 << CS00);

	OCR0A = 0;
}

void timer1_init(void)
{

	TCCR1A = (1 << COM1A1) |
	(1 << WGM11);

	TCCR1B = (1 << WGM13) |
	(1 << WGM12) |
	(1 << CS11);

	ICR1 = 39999;

	OCR1A = 2000;
}

void spi_esclavo_init(void)
{

	DDRB |= (1 << SPI_MISO);

	DDRB &= ~((1 << SPI_SS) |
	(1 << SPI_MOSI) |
	(1 << SPI_SCK));

	PORTB |= (1 << SPI_SS);

	SPCR = (1 << SPE) |
	(1 << SPIE);
}
void indicar_paro(void)
{
	PORTB &= ~(1 << LED_VERDE);

	PORTC |= (1 << LED_ROJO);

	PORTC &= ~(1 << BUZZER);
}

void indicar_marcha(void)
{
	PORTB |= (1 << LED_VERDE);

	PORTC &= ~(1 << LED_ROJO);

	PORTC &= ~(1 << BUZZER);
}

void indicar_emergencia(void)
{
	PORTB &= ~(1 << LED_VERDE);

	PORTC |= (1 << LED_ROJO);

	PORTC |= (1 << BUZZER);
}

ISR(SPI_STC_vect)
{
	uint8_t dato = SPDR;
	if (fase_spi == RECIBIR_COMANDO)
	{
		comando_recibido = dato;
		fase_spi = RECIBIR_VALOR;
	}
	else
	{
		valor_recibido = dato;
		fase_spi = RECIBIR_COMANDO;
		if (comando_recibido == CMD_EMERGENCY)
		{
			estado = EST_EMERGENCIA;
			motor_detener();
			indicar_emergencia();
			OCR1A = 2000;
			tiempo_servo = 0;
		}

		else if (comando_recibido == CMD_STOP)
		{
			estado = EST_PARADO;
			motor_detener();
			indicar_paro();
			OCR1A = 2000;
			tiempo_servo = 0;
		}

		// ---------------------------------------------
		// RESTO DE COMANDOS
		// ---------------------------------------------

		else if (estado != EST_EMERGENCIA)
		{
			switch (comando_recibido)
			{
				case CMD_START:
				estado = EST_MARCHA;
				indicar_marcha();
				PORTD |= (1 << MOTOR_IN1);
				PORTD &= ~(1 << MOTOR_IN2);
				break;
				case CMD_SPEED:
				if (estado == EST_MARCHA ||
				estado == EST_PISTON)
				{
					motor_velocidad(valor_recibido);
				}

				break;
				case CMD_PISTON:
				if (estado == EST_MARCHA ||
				estado == EST_PISTON)
				{
					estado = EST_PISTON;
					OCR1A = 4000;
					tiempo_servo = 700;
				}
				break;
				default:
				break;
			}
		}
	}
}

