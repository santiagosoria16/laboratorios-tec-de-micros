#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

// Dirección I2C del PCF8574 (Cambiar a 0x3F si la pantalla LCD no responde)
#define PCF8574_ADDR 0x27

// ==========================================
// DEFINICIÓN DE FRECUENCIAS (Hz)
// ==========================================
#define SIL 0

// Octava 2
#define B2  123

// Octava 3 - Bajos / Acompañamiento
#define C3  131
#define CS3 139
#define D3  147
#define DS3 156
#define EB3 156
#define E3  165
#define F3  175
#define FS3 185
#define G3  196
#define GS3 208
#define AB3 208
#define A3  220
#define AS3 233
#define BB3 233
#define B3  247

// Octava 4 - Melodía
#define C4  262
#define CS4 277
#define D4  294
#define DS4 311
#define EB4 311
#define E4  330
#define F4  349
#define FS4 370
#define G4  392
#define GS4 415
#define AB4 415
#define A4  440
#define AS4 466
#define BB4 466
#define B4  494

// Octava 5
#define C5  523
#define CS5 554
#define D5  587
#define DS5 622
#define EB5 622
#define E5  659
#define F5  698
#define FS5 740
#define G5  784
#define GS5 831
#define A5  880
#define AS5 932
#define BB5 932
#define B5  988

// Octava 6
#define C6  1047
#define D6  1175

#define FREQ_TO_ICR1(f) ((f > 0) ? ((F_CPU / (8UL * (f))) - 1) : 0)

// ==========================================
// ESTADOS Y MENÚ
// ==========================================
typedef enum {
	ESTADO_MENU,
	ESTADO_PIANO,
	ESTADO_CANCION
} EstadoSistema;
