#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

// ---------------- Configuración ----------------
#define BAUD            115200UL
#define UBRR_VALUE      ((F_CPU / (8UL * BAUD)) - 1)   // con U2X0 = 1 (error ~2.1 %)

#define OVERSAMPLE      16          // 16 * 1023 = 16368 -> entra en uint16_t
#define USAR_REF_INTERNA 0          // 0: AVCC (~5 V)   1: referencia interna 1.1 V
                                    // Con 1.1 V la salida del op-amp NO debe superar 1.1 V
                                    // (con shunt 4.7 ohm y Av = 21 -> máx ~11 mA, incluido el offset)

