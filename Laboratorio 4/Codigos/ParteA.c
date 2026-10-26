#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define LED_VERDE_PIN   PD6
#define LED_ROJO_PIN    PD7
#define BUZZER_PIN      PB0


#define SPI_SS          PB2
#define SPI_MOSI        PB3
#define SPI_MISO        PB4
#define SPI_SCK         PB5 
#define RC522_RST       PB1 

#define PCF8574_ADDR    (0x27 << 1)

#define LCD_RS          (1 << 0)
#define LCD_EN          (1 << 2)
#define LCD_BACKLIGHT   (1 << 3)

#define EEPROM_MAGIC_ADDR  0x03FF
#define EEPROM_MAGIC_VAL   0xA6

