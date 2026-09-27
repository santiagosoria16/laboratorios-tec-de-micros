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

  ldi R_LOOP, 8               ; 8 x 2.5s = 20 segundos exactos
G_LEFT_LOOP:
    rcall DELAY_FAR_LEFT
    dec R_LOOP
    brne G_LEFT_LOOP

    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY
    ret

; Subrutina de salto de Fila 1 a Fila 2 (Vuelve a la izquierda 20s y baja)
MOVE_TO_ROW2:
    rcall PEN_UP

    ; Retorno hacia la izquierda (20s)
    ldi R_TEMP, MOVE_LEFT
    rcall MOVE_RAW_DIRECT

    ldi R_LOOP, 8               ; 8 x 2.5s = 20 segundos exactos
M_ROW2_LEFT:
    rcall DELAY_FAR_LEFT
    dec R_LOOP
    brne M_ROW2_LEFT

    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY

    ; Bajar a la Fila 2 (80 pasos en -Y para no encimar las figuras)
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 80
    rcall DO_N_STEPS

    rcall DELAY_MEDIUM
    ret

; Regreso a HOME desde la Fila 2
RETURN_TO_HOME_ROW2:
    rcall PEN_UP

    ; Compensar la bajada de la Fila 2 subiendo 80 pasos
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 80
    rcall DO_N_STEPS

; Ejecutar el regreso estándar a HOME
    rcall RETURN_TO_HOME
    ret

RETURN_TO_HOME:
    rcall PEN_UP

    ldi R_TEMP, MOVE_RIGHT
    rcall MOVE_RAW_DIRECT

    ldi R_LOOP, 8               ; 8 x 2.5s = 20 segundos exactos
R_HOME_LOOP:
    rcall DELAY_FAR_LEFT
    dec R_LOOP
    brne R_HOME_LOOP

    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY

    ldi R_TEMP, (MOVE_RIGHT | MOVE_UP)
    rcall MOVE_RAW_DIRECT
    rcall DELAY_CENTER_Y
    ldi R_TEMP, 0x00
    rcall MOVE_RAW_DIRECT
    rcall DELAY_RELAY
    ret

SHIFT_RIGHT_3CM:
    rcall PEN_UP
    ldi R_LOOP, 85              ; ~3.2 cm de separación a la derecha
S_RIGHT_LOOP:
    ldi R_TEMP, MOVE_RIGHT
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne S_RIGHT_LOOP
    rcall DELAY_MEDIUM
    ret

MOVE_RAW_DIRECT:
    in R_DELAY2, PORTD
    andi R_DELAY2, (PEN_DOWN_BIT | PEN_UP_BIT)
    or R_TEMP, R_DELAY2
    out PORTD, R_TEMP
    ret

; ==============================================================================
; SUBRUTINAS DE DIBUJO DE FIGURAS
; ==============================================================================

; ------------------------------------------------------------------------------
; TRIÁNGULO
; ------------------------------------------------------------------------------
DRAW_TRIANGLE:
    rcall PEN_DOWN

    ldi R_LOOP, 54
T_STEP1:
    ldi R_TEMP, MOVE_DOWN
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne T_STEP1

    ldi R_LOOP, 54
T_STEP2:
    ldi R_TEMP, MOVE_RIGHT
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne T_STEP2

    ldi R_LOOP, 54
T_STEP3:
    ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne T_STEP3

    rcall PEN_UP
    ret

; ==============================================================================
; CÍRCULO MINECRAFT PERFECTO (R = 30 pasos, 30x30 por cuadrante)
; ==============================================================================
DRAW_CIRCLE:
    rcall PEN_UP

    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 30
    rcall DO_N_STEPS

    rcall PEN_DOWN

    ; CUADRANTE 1
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 5
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 4
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 2
    rcall DO_N_STEPS

ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    ldi R_LOOP, 8
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 5
    rcall DO_N_STEPS

 ; CUADRANTE 2
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 5
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS

 ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    ldi R_LOOP, 8
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 4
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 5
    rcall DO_N_STEPS

 ; CUADRANTE 3
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 5
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 4
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 2
    rcall DO_N_STEPS

 ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    ldi R_LOOP, 8
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 5
    rcall DO_N_STEPS

    ; CUADRANTE 4
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 5
    rcall DO_N_STEPS

 ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 3
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 3
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 2
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 2
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 2
    rcall DO_N_STEPS

    ldi R_TEMP, (MOVE_UP | MOVE_RIGHT)
    ldi R_LOOP, 8
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 1
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 3
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 2
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 4
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 1
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 5
    rcall DO_N_STEPS

    rcall PEN_UP

    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 30
    rcall DO_N_STEPS

    ret

; ------------------------------------------------------------------------------
; SUBRUTINA AUXILIAR PARA PASOS REPETITIVOS
; ------------------------------------------------------------------------------
DO_N_STEPS:
    push R_TEMP
    rcall MOVE_SUPER_SHORT_STEP
    pop R_TEMP
    dec R_LOOP
    brne DO_N_STEPS
    ret

; ------------------------------------------------------------------------------
; PENTAGRAMA
; ------------------------------------------------------------------------------

DRAW_PENTAGRAM:
    rcall PEN_DOWN

    ldi R_LOOP, 31
L_STEEP_DR:
    ldi R_TEMP, MOVE_DOWN
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, MOVE_DOWN
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne L_STEEP_DR

    ldi R_LOOP, 27
L_SHALLOW_UL:
    ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, MOVE_LEFT
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne L_SHALLOW_UL

    ldi R_LOOP, 100
L_HORIZ_R:
    ldi R_TEMP, MOVE_RIGHT
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne L_HORIZ_R

  ldi R_LOOP, 27
L_SHALLOW_DL:
    ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, MOVE_LEFT
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne L_SHALLOW_DL

    ldi R_LOOP, 31
L_STEEP_UR:
    ldi R_TEMP, MOVE_UP
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, MOVE_UP
    rcall MOVE_SUPER_SHORT_STEP
    ldi R_TEMP, (MOVE_UP | MOVE_RIGHT)
    rcall MOVE_SUPER_SHORT_STEP
    dec R_LOOP
    brne L_STEEP_UR

    rcall PEN_UP
    ret

; ------------------------------------------------------------------------------
; FIGURA LIBRE: LA CASITA
; ------------------------------------------------------------------------------

DRAW_FREE_FIG:
    rcall PEN_UP

    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 20
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 20
    rcall DO_N_STEPS

    rcall PEN_DOWN

    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 40
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 30
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 40
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 30
    rcall DO_N_STEPS

    ldi R_TEMP, (MOVE_UP | MOVE_RIGHT)
    ldi R_LOOP, 20
    rcall DO_N_STEPS

    ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    ldi R_LOOP, 20
    rcall DO_N_STEPS

    rcall PEN_UP

    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 20
    rcall DO_N_STEPS

    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 10
    rcall DO_N_STEPS

    ret
; ==============================================================================
; POKÉMON: SHELLDER (#090) - VERSIÓN CORREGIDA Y ALINEADA (ESCALA GRANDE)
; Centrado exacto en (0,0)
; ==============================================================================
DRAW_POKEMON:
    rcall PEN_UP

    ; --------------------------------------------------------------------------
    ; 1. CORONA / BISAGRA SUPERIOR (X: -10 a +10, Y: +30 a +36)
    ; --------------------------------------------------------------------------
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 10
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 30
    rcall DO_N_STEPS

    rcall PEN_DOWN
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 20
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 20
    rcall DO_N_STEPS
    rcall PEN_UP

  ; --------------------------------------------------------------------------
    ; 2. CONCHA SUPERIOR Y CUERNOS GRANDES
    ; --------------------------------------------------------------------------
    ; Ir al inicio del domo superior (-16, 28)
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 2
    rcall DO_N_STEPS

    rcall PEN_DOWN

    ; Arco a la base del cuerno izquierdo
    ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    ldi R_LOOP, 6
    rcall DO_N_STEPS

    ; Cuerno Izquierdo (Punta hacia afuera)
    ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    ldi R_LOOP, 12
    rcall DO_N_STEPS
    ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    ldi R_LOOP, 14
    rcall DO_N_STEPS

    ; Borde exterior izquierdo
    ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    ldi R_LOOP, 8
    rcall DO_N_STEPS

    ; Visera / Borde inferior curvo de la concha (Cubre la cara)
    ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    ldi R_LOOP, 4
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 48
    rcall DO_N_STEPS
    ldi R_TEMP, (MOVE_UP | MOVE_RIGHT)
    ldi R_LOOP, 4
    rcall DO_N_STEPS

    ; Borde exterior derecho
    ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    ldi R_LOOP, 8
    rcall DO_N_STEPS

    ; Cuerno Derecho (Punta hacia afuera)
    ldi R_TEMP, (MOVE_UP | MOVE_RIGHT)
    ldi R_LOOP, 14
    rcall DO_N_STEPS
    ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    ldi R_LOOP, 12
    rcall DO_N_STEPS

    ; Domo superior
    ldi R_TEMP, (MOVE_UP | MOVE_LEFT)
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 32
    rcall DO_N_STEPS
    rcall PEN_UP

  ; --------------------------------------------------------------------------
    ; 3. CRESTAS VERTICALES DE LA CONCHA
    ; --------------------------------------------------------------------------
    ; Cresta Central (0, 28) -> (0, 8)
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 16
    rcall DO_N_STEPS

    rcall PEN_DOWN
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 20
    rcall DO_N_STEPS
    rcall PEN_UP

    ; Cresta Izquierda (-10, 26) -> (-16, 8)
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 10
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 18
    rcall DO_N_STEPS

    rcall PEN_DOWN
    ldi R_TEMP, (MOVE_DOWN | MOVE_LEFT)
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 12
    rcall DO_N_STEPS
    rcall PEN_UP

    ; Cresta Derecha (+10, 26) -> (+16, 8)
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 26
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 18
    rcall DO_N_STEPS

    rcall PEN_DOWN
    ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 12
    rcall DO_N_STEPS
    rcall PEN_UP

 ; --------------------------------------------------------------------------
    ; 4. CAVIDAD DE LA CARA (JUSTO DEBAJO DE LA VISERA)
    ; --------------------------------------------------------------------------
    ; Ir a (-24, 8)
    ldi R_TEMP, MOVE_LEFT
    ldi R_LOOP, 40
    rcall DO_N_STEPS

    rcall PEN_DOWN
    ldi R_TEMP, MOVE_DOWN
    ldi R_LOOP, 8
    rcall DO_N_STEPS
    ldi R_TEMP, (MOVE_DOWN | MOVE_RIGHT)
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_RIGHT
    ldi R_LOOP, 36
    rcall DO_N_STEPS
    ldi R_TEMP, (MOVE_UP | MOVE_RIGHT)
    ldi R_LOOP, 6
    rcall DO_N_STEPS
    ldi R_TEMP, MOVE_UP
    ldi R_LOOP, 8
    rcall DO_N_STEPS
    rcall PEN_UP







