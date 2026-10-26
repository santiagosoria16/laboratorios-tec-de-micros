
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

