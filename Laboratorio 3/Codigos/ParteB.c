#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#define BAUD 9600
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

// Definición de pines del Puente H
#define IN1_PIN PD2
#define IN2_PIN PD3
#define PWM_PIN PD6
