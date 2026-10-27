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

