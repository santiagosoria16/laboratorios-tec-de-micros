
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

