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

