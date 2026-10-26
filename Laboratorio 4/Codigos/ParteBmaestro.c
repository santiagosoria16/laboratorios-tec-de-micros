
#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>

#define CMD_START       0x01
#define CMD_STOP        0x02
#define CMD_SPEED       0x03
#define CMD_PISTON      0x04
#define CMD_EMERGENCY   0x05

#define TEMP_LIMITE     30

#define PIN_LDR         PD1
#define PIN_BOTON       PD2
#define PIN_DHT         PD3

#define PIN_SS          PB2
#define PIN_MOSI        PB3
#define PIN_MISO        PB4
#define PIN_SCK         PB5

#define LCD_ADDR        0x27

#define LCD_RS_BIT      0
#define LCD_EN_BIT      2
#define LCD_BL_BIT      3
typedef enum {
	EST_PARADO = 0,
	EST_FUNCIONANDO,
	EST_DESVIANDO,
	EST_FALLA
} estado_maestro_t;

volatile uint8_t evento_boton = 0;

estado_maestro_t estado = EST_PARADO;

uint16_t piezas = 0;

uint8_t pwm_actual = 0;
uint8_t pwm_anterior = 255;

uint8_t temperatura = 25;

uint8_t ldr_referencia = 0;
uint8_t ldr_bloqueado = 0;

const char *comando_actual = "STOP";


void i2c_init(void)
{
	TWSR = 0;
	TWBR = 72;
	TWCR = (1 << TWEN);
}

void i2c_start(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

	while (!(TWCR & (1 << TWINT)));
}

void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void i2c_write(uint8_t dato)
{
	TWDR = dato;
	TWCR = (1 << TWINT) | (1 << TWEN);

	while (!(TWCR & (1 << TWINT)));
}

void pcf_escribir(uint8_t dato)
{
	i2c_start();

	i2c_write(LCD_ADDR << 1);
	i2c_write(dato);

	i2c_stop();
}

void lcd_nibble(uint8_t nibble, uint8_t rs)
{
	uint8_t dato = (nibble << 4) |
	(1 << LCD_BL_BIT);

	if (rs)
	dato |= (1 << LCD_RS_BIT);

	pcf_escribir(dato | (1 << LCD_EN_BIT));

	_delay_us(2);

	pcf_escribir(dato);

	_delay_us(50);
}

void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void i2c_write(uint8_t dato)
{
	TWDR = dato;
	TWCR = (1 << TWINT) | (1 << TWEN);

	while (!(TWCR & (1 << TWINT)));
}

void pcf_escribir(uint8_t dato)
{
	i2c_start();

	i2c_write(LCD_ADDR << 1);
	i2c_write(dato);

	i2c_stop();
}

void lcd_nibble(uint8_t nibble, uint8_t rs)
{
	uint8_t dato = (nibble << 4) |
	(1 << LCD_BL_BIT);

	if (rs)
	dato |= (1 << LCD_RS_BIT);

	pcf_escribir(dato | (1 << LCD_EN_BIT));

	_delay_us(2);

	pcf_escribir(dato);

	_delay_us(50);
}

void adc_init(void)
{
	ADMUX = (1 << REFS0);

	ADCSRA = (1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);
}

uint16_t adc_leer(uint8_t canal)
{
	ADMUX = (1 << REFS0) | (canal & 0x07);

	ADCSRA |= (1 << ADSC);

	while (ADCSRA & (1 << ADSC));

	return ADC;
}

void spi_init(void)
{
	DDRB |= (1 << PIN_SS) |
	(1 << PIN_MOSI) |
	(1 << PIN_SCK);

	DDRB &= ~(1 << PIN_MISO);

	PORTB |= (1 << PIN_SS);

	SPCR = (1 << SPE) |
	(1 << MSTR) |
	(1 << SPR1);
}

void spi_enviar_byte(uint8_t dato)
{
	SPDR = dato;

	while (!(SPSR & (1 << SPIF)));
}

void spi_enviar_trama(uint8_t comando, uint8_t valor)
{
	PORTB &= ~(1 << PIN_SS);

	_delay_us(200);

	spi_enviar_byte(comando);

	_delay_us(300);

	spi_enviar_byte(valor);

	_delay_us(200);

	PORTB |= (1 << PIN_SS);

	_delay_ms(2);
}

ISR(INT0_vect)
{
	evento_boton = 1;
}

void boton_init(void)
{
	DDRD &= ~(1 << PIN_BOTON);

	PORTD |= (1 << PIN_BOTON);

	EICRA = (1 << ISC01);

	EIMSK = (1 << INT0);
}

uint8_t dht_esperar(uint8_t nivel)
{
	uint16_t timeout = 1200;

	while (((PIND & (1 << PIN_DHT)) != 0) != nivel)
	{
		if (--timeout == 0)
		return 0;
	}

	return 1;
}

uint8_t dht_leer(void)
{
	uint8_t datos[5] = {0, 0, 0, 0, 0};

	uint8_t i, j;
	uint8_t resultado = 0;

	uint8_t estado_interrupciones = SREG;

	// Pulso de inicio
	DDRD |= (1 << PIN_DHT);

	PORTD &= ~(1 << PIN_DHT);

	_delay_ms(18);

	PORTD |= (1 << PIN_DHT);

	_delay_us(30);

	DDRD &= ~(1 << PIN_DHT);

	PORTD |= (1 << PIN_DHT);

	cli();

	if (!dht_esperar(0)) goto salida;
	if (!dht_esperar(1)) goto salida;
	if (!dht_esperar(0)) goto salida;


	for (i = 0; i < 5; i++)
	{
		for (j = 0; j < 8; j++)
		{
			if (!dht_esperar(1)) goto salida;

			_delay_us(40);

			if (PIND & (1 << PIN_DHT))
			datos[i] |= (1 << (7 - j));

			if (!dht_esperar(0)) goto salida;
		}
	}


	if ((uint8_t)(datos[0] + datos[1] +
	datos[2] + datos[3]) == datos[4])
	{
		resultado = datos[2];
	}

	salida:

	SREG = estado_interrupciones;

	return resultado;
}

int main(void)
{
	char linea[17];

	uint8_t boton_actual;
	uint8_t boton_presionado;

	uint8_t ldr_actual;

	uint8_t ciclos_dht = 0;


	DDRD &= ~((1 << PIN_LDR) | (1 << PIN_BOTON));

	PORTD |= (1 << PIN_BOTON);


	DDRC &= ~(1 << PC0);


	boton_init();

	adc_init();

	i2c_init();

	spi_init();

	lcd_init();

	sei();

	lcd_texto(0, 0, "SISTEMA LISTO");

	_delay_ms(700);

	ldr_referencia = (PIND & (1 << PIN_LDR)) ? 1 : 0;

	estado = EST_PARADO;

	spi_enviar_trama(CMD_STOP, 0);

	while (1)
	{

		boton_presionado = 0;

		if (evento_boton)
		{
			evento_boton = 0;

			_delay_ms(25);

			if (!(PIND & (1 << PIN_BOTON)))
			{
				boton_presionado = 1;

				while (!(PIND & (1 << PIN_BOTON)))
				_delay_ms(5);
			}
		}


		pwm_actual = (uint8_t)(adc_leer(0) >> 2);

		ldr_actual = (PIND & (1 << PIN_LDR)) ? 1 : 0;

		ciclos_dht++;

		if (ciclos_dht >= 35)
		{
			ciclos_dht = 0;

			uint8_t nueva_temp = dht_leer();

			if (nueva_temp > 0 && nueva_temp < 95)
			temperatura = nueva_temp;
		}

		switch (estado)
		{
			case EST_PARADO:

			if (temperatura >= TEMP_LIMITE)
			{
				spi_enviar_trama(CMD_EMERGENCY, 0xFF);

				comando_actual = "EMERG";

				estado = EST_FALLA;
			}
			else if (boton_presionado)
			{
				ldr_referencia = ldr_actual;

				ldr_bloqueado = 0;

				spi_enviar_trama(CMD_START, 0);

				spi_enviar_trama(CMD_SPEED, pwm_actual);

				pwm_anterior = pwm_actual;

				comando_actual = "START";

				estado = EST_FUNCIONANDO;
			}

			break;

			case EST_FUNCIONANDO:

			if (temperatura >= TEMP_LIMITE)
			{
				spi_enviar_trama(CMD_EMERGENCY, 0xFF);

				comando_actual = "EMERG";

				estado = EST_FALLA;
			}
			else if (boton_presionado)
			{
				spi_enviar_trama(CMD_STOP, 0);

				comando_actual = "STOP";

				estado = EST_PARADO;
			}
			else
			{


				if (ldr_actual != ldr_referencia)
				{
					if (!ldr_bloqueado)
					estado = EST_DESVIANDO;
				}
				else
				{
					ldr_bloqueado = 0;
				}


				int16_t diferencia =
				(int16_t)pwm_actual - (int16_t)pwm_anterior;

				if (diferencia >= 2 || diferencia <= -2)
				{
					spi_enviar_trama(CMD_SPEED, pwm_actual);

					pwm_anterior = pwm_actual;

					comando_actual = "SPEED";
				}
			}

			break;

			case EST_DESVIANDO:

			ldr_bloqueado = 1;

			piezas++;

			spi_enviar_trama(CMD_PISTON, 1);

			comando_actual = "PISTON";

			estado = EST_FUNCIONANDO;

			break;

			case EST_FALLA:

			if (boton_presionado && temperatura < TEMP_LIMITE)
			{
				spi_enviar_trama(CMD_STOP, 0);

				comando_actual = "STOP";

				estado = EST_PARADO;
			}

			break;

			default:

			estado = EST_PARADO;

			break;
		}

