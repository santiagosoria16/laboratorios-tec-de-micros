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

MAIN_LOOP:
    rcall PRINT_MENU

WAIT_COMMAND:
    rcall USART_RECEIVE

    cpi R_DATA, '1'
    breq DO_TRIANGLE

    cpi R_DATA, '2'
    breq DO_CIRCLE

    cpi R_DATA, '3'
    breq DO_PENTAGRAM

    cpi R_DATA, '4'
    breq DO_FREE_FIG

    cpi R_DATA, 'P'
    breq DO_POKEMON
    cpi R_DATA, 'p'
    breq DO_POKEMON

    cpi R_DATA, 'T'
    breq DO_ALL
    cpi R_DATA, 't'
    breq DO_ALL

    rjmp WAIT_COMMAND

; ------------------------------------------------------------------------------
; RUTINAS INTERMEDIAS DE SALTO
; ------------------------------------------------------------------------------

DO_TRIANGLE:
    rcall GO_TO_CENTER
    rcall DRAW_TRIANGLE
    rcall RETURN_TO_HOME
    rjmp MAIN_LOOP

DO_CIRCLE:
    rcall GO_TO_CENTER
    rcall DRAW_CIRCLE
    rcall RETURN_TO_HOME
    rjmp MAIN_LOOP

DO_PENTAGRAM:
    rcall GO_TO_CENTER
    rcall DRAW_PENTAGRAM
    rcall RETURN_TO_HOME
    rjmp MAIN_LOOP

DO_FREE_FIG:
    rcall GO_TO_CENTER
    rcall DRAW_FREE_FIG
    rcall RETURN_TO_HOME
    rjmp MAIN_LOOP

DO_POKEMON:
    rcall GO_TO_CENTER
    rcall DRAW_POKEMON
    rcall RETURN_TO_HOME
    rjmp MAIN_LOOP

; ------------------------------------------------------------------------------
; SECUENCIA COMPLETA EN 2 FILAS
; ------------------------------------------------------------------------------
DO_ALL:
    rcall GO_TO_FAR_LEFT        ; Posiciona en el tope izquierdo (Fila 1)

    ; --- FILA 1 (ARRIBA) ---
    rcall DRAW_TRIANGLE
    rcall SHIFT_RIGHT_3CM

    rcall DRAW_CIRCLE
    rcall SHIFT_RIGHT_3CM

    rcall DRAW_PENTAGRAM

    ; --- SALTO A FILA 2 (RETORNO A LA IZQUIERDA Y BAJADA) ---
    rcall MOVE_TO_ROW2

    ; --- FILA 2 (ABAJO) ---
    rcall DRAW_FREE_FIG
    rcall SHIFT_RIGHT_3CM

    rcall DRAW_POKEMON

    ; --- REGRESO A HOME DESDE FILA 2 ---
    rcall RETURN_TO_HOME_ROW2
    rjmp MAIN_LOOP

; ==============================================================================
; POSICIONAMIENTO Y DESPLAZAMIENTOS
; ==============================================================================
GO_TO_CENTER:
    rcall PEN_UP

    ldi R_TEMP, (MOVE_LEFT | MOVE_DOWN)
    rcall MOVE_RAW_DIRECT
    rcall DELAY_CENTER_Y
    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY

    ldi R_TEMP, MOVE_LEFT
    rcall MOVE_RAW_DIRECT
    rcall DELAY_CENTER_X_EXTRA
    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY
    ret

; Posiciona en el tope izquierdo (8 ciclos x 2.5s = EXATOS 20s)
GO_TO_FAR_LEFT:
    rcall PEN_UP

    ldi R_TEMP, (MOVE_LEFT | MOVE_DOWN)
    rcall MOVE_RAW_DIRECT
    rcall DELAY_CENTER_Y
    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY

    ldi R_TEMP, MOVE_LEFT
    rcall MOVE_RAW_DIRECT










