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

