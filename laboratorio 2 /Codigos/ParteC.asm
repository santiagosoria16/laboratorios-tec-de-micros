; ==============================================================================
; PROYECTO: CONTROL DE PLOTTER MEDIANTE ATMEGA328P Y USART
; ASIGNATURA: Tecnologías de Microprocesamiento - UTEC
; DESCRIPCIÓN: Círculo perfecto Minecraft (30x30), Pokémon continuo (Shellder),
;              Casita, Triángulo, Pentagrama.
;              Organización en 2 FILAS con desplazamiento exacto de 20s a la izquierda.
; ==============================================================================

.include "m328pdef.inc"

; ------------------------------------------------------------------------------
; DEFINICIÓN DE REGISTROS
; ------------------------------------------------------------------------------
.def R_TEMP   = r16    ; Registro temporal de propósito general
.def R_DATA   = r17    ; Registro de datos para recepción y transmisión USART
.def R_DELAY1 = r18    ; Contador Nivel 1 para subrutinas de retardo
.def R_DELAY2 = r19    ; Contador Nivel 2 para subrutinas de retardo
.def R_DELAY3 = r20    ; Contador Nivel 3 para subrutinas de retardo
.def R_LOOP   = r21    ; Contador de repeticiones de pasos

; ------------------------------------------------------------------------------
; CONSTANTES Y MÁSCARAS DE PINES
; ------------------------------------------------------------------------------
.equ UBRR_VAL = 103

.equ PEN_DOWN_BIT = (1 << PD2)  ; D2 -> X0: Bajar solenoide / Habilitar trazo
.equ PEN_UP_BIT   = (1 << PD3)  ; D3 -> X1: Subir solenoide / Deshabilitar trazo
.equ MOVE_DOWN    = (1 << PD4)  ; D4 -> X5: Mover hacia abajo (-Y)
.equ MOVE_UP      = (1 << PD5)  ; D5 -> X6: Mover hacia arriba (+Y)
.equ MOVE_LEFT    = (1 << PD7)  ; D6 -> X7: Mover hacia la izquierda (-X)
.equ MOVE_RIGHT   = (1 << PD6)  ; D7 -> X10: Mover hacia la derecha (+X)

; ------------------------------------------------------------------------------
; VECTOR DE INTERRUPCIONES
; ------------------------------------------------------------------------------
.org 0x0000
    rjmp RESET

; ------------------------------------------------------------------------------
; INICIALIZACIÓN DEL SISTEMA
; ------------------------------------------------------------------------------
RESET:
    ldi R_TEMP, HIGH(RAMEND)
    out SPH, R_TEMP
    ldi R_TEMP, LOW(RAMEND)
    out SPL, R_TEMP

    ldi R_TEMP, 0xFC             ; Bits 2-7 como salidas
    out DDRD, R_TEMP
    ldi R_TEMP, PEN_UP_BIT       ; Estado inicial: Lápiz arriba
    out PORTD, R_TEMP

    ldi R_TEMP, HIGH(UBRR_VAL)
    sts UBRR0H, R_TEMP
    ldi R_TEMP, LOW(UBRR_VAL)
    sts UBRR0L, R_TEMP

    ldi R_TEMP, (1 << RXEN0) | (1 << TXEN0)
    sts UCSR0B, R_TEMP

    ldi R_TEMP, (1 << UCSZ01) | (1 << UCSZ00)
    sts UCSR0C, R_TEMP

; ------------------------------------------------------------------------------
; BUCLE PRINCIPAL Y MENÚ DE COMANDOS
; ------------------------------------------------------------------------------



























