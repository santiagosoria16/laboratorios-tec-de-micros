#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#define HOLD_PIN   PD2
#define CALIB_PIN  PD3
#define BUZZER_PIN PD4
#define LED_PIN    PD6

#define MPU6050_ADDR 0xD0
#define LCD_ADDR     0x4E

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01

#define EEPROM_MAGIC 0xA5
uint8_t EEMEM eeprom_flag;
float EEMEM eeprom_offset_x;
float EEMEM eeprom_offset_y;

float offset_x = 0.0f;
float offset_y = 0.0f;

uint8_t led_buffer[192];

