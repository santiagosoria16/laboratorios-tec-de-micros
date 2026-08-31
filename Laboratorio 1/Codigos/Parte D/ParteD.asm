
.include "m328pdef.inc"


; DEFINICIÓN DE REGISTROS DE PROPÓSITO GENERAL

.def R_A      = r16  ; Operando A (4 bits)
.def R_B      = r17  ; Operando B (4 bits)
.def R_F      = r18  ; Resultado F (4 bits)
.def FLAG_C   = r19  ; Acarreo (Carry)
.def FLAG_N   = r20  ; Negativo (Negative)
.def FLAG_Z   = r21  ; Cero (Zero)
.def R_SEL    = r22  ; Código de Selección S (3 bits: 0..7)
.def R_OUT    = r23  ; Byte final de salida para PORTD
.def R_TEMP1  = r24  ; Registro temporal de lectura 1
.def R_TEMP2  = r25  ; Registro temporal de lectura 2


; VECTOR DE RESET

.org 0x0000
    rjmp SETUP

; CONFIGURACIÓN DE PUERTOS E INICIALIZACIÓN

SETUP:
    ; Inicialización del Puntero de Pila (Stack Pointer)
    ldi R_TEMP1, LOW(RAMEND)
    out SPL, R_TEMP1
    ldi R_TEMP1, HIGH(RAMEND)
    out SPH, R_TEMP1

    ; Configurar PORTD completo como SALIDA (Resultado F, Flags C/N/Z e Indicador)
    ldi R_TEMP1, 0xFF
    out DDRD, R_TEMP1
    clr R_TEMP1
    out PORTD, R_TEMP1

    ; Configurar PORTC como ENTRADA (Operando A: PC0..PC3 | S0, S1: PC4, PC5)
    clr R_TEMP1
    out DDRC, R_TEMP1

    ; Configurar PORTB como ENTRADA (Operando B: PB0..PB3 | S2: PB4)
    clr R_TEMP1
    out DDRB, R_TEMP1


; BUCLE PRINCIPAL (MAIN LOOP)

MAIN_LOOP:
    ;  LECTURA DE PUERTO C (Operando A y bits S0, S1) 
    in R_TEMP1, PINC


    ; Extraer Operando A (PC0..PC3)
    mov R_A, R_TEMP1
    andi R_A, 0x0F

    ; Extraer bits S0 y S1 (PC4..PC5) y alinear a los bits 0 y 1
    mov R_SEL, R_TEMP1
    lsr R_SEL
    lsr R_SEL
    lsr R_SEL
    lsr R_SEL
    andi R_SEL, 0x03    ; R_SEL = [0000 00 S1 S0]

    ; LECTURA DE PUERTO B (Operando B y bit S2) 
    in R_TEMP2, PINB
    
    ; Extraer Operando B (PB0..PB3)
    mov R_B, R_TEMP2
    andi R_B, 0x0F

    ; Extraer bit S2 (PB4) y colocarlo en el bit 2 de R_SEL
    sbrc R_TEMP2, 4
    ori R_SEL, 0x04     ; R_SEL = [0000 0 S2 S1 S0] (Valor 0 a 7)

    ; DECODIFICACIÓN Y EJECUCIÓN SEGÚN LA TABLA 
    cpi R_SEL, 0
    breq EXEC_CLEAR
    cpi R_SEL, 1
    breq EXEC_SUB
    cpi R_SEL, 2
    breq EXEC_ADD
    cpi R_SEL, 3
    breq EXEC_XOR
    cpi R_SEL, 4
    breq EXEC_AND
    cpi R_SEL, 5
    breq EXEC_OR
    cpi R_SEL, 6
    breq EXEC_SHL
    cpi R_SEL, 7
    breq EXEC_INC

    rjmp MAIN_LOOP


; RUTINAS DE OPERACIONES DE LA ALU


; S = 000 (0): CLEAR -> Resultado F = 00
EXEC_CLEAR:
    clr R_F
    clr FLAG_C
    rjmp CALC_FLAGS

; S = 001 (1): A - B
EXEC_SUB:
    mov R_F, R_A
    sub R_F, R_B
    clr FLAG_C
    brcc SUB_NO_CARRY
    ldi FLAG_C, 1       ; A < B produjo préstamo/overflow -> Carry = 1
SUB_NO_CARRY:
    andi R_F, 0x0F
    rjmp CALC_FLAGS

; S = 010 (2): A + B
EXEC_ADD:
    mov R_F, R_A
    add R_F, R_B
    clr FLAG_C
    cpi R_F, 16
    brlo ADD_NO_CARRY
    ldi FLAG_C, 1       ; Resultado >= 16 supera 4 bits -> Carry = 1
ADD_NO_CARRY:
    andi R_F, 0x0F
    rjmp CALC_FLAGS

; S = 011 (3): XOR (A XOR B)
EXEC_XOR:
    mov R_F, R_A
    eor R_F, R_B
    andi R_F, 0x0F
    clr FLAG_C
    rjmp CALC_FLAGS

; S = 100 (4): AND (A AND B)
EXEC_AND:
    mov R_F, R_A
    and R_F, R_B
    andi R_F, 0x0F
    clr FLAG_C
    rjmp CALC_FLAGS

; S = 101 (5): OR (A OR B)
EXEC_OR:
    mov R_F, R_A
    or R_F, R_B
    andi R_F, 0x0F
    clr FLAG_C
    rjmp CALC_FLAGS

; S = 110 (6): SHL (A << 1)
EXEC_SHL:
    clr FLAG_C
    sbrc R_A, 3         ; Si el bit MSB de A (bit 3) está en 1, pasa al Carry
    ldi FLAG_C, 1
    mov R_F, R_A
    lsl R_F
    andi R_F, 0x0F
    rjmp CALC_FLAGS

; S = 111 (7): INC (A + 1)
EXEC_INC:
    mov R_F, R_A
    inc R_F
    clr FLAG_C
    cpi R_F, 16
    brlo INC_NO_CARRY
    ldi FLAG_C, 1       ; 15 + 1 = 16 (supera nibble de 4 bits) -> Carry = 1
INC_NO_CARRY:
    andi R_F, 0x0F
    rjmp CALC_FLAGS


; CÁLCULO DE BANDERAS N (NEGATIVO) Y Z (CERO)

CALC_FLAGS:
    ; Flag N: 1 si el bit 3 de F es 1 (signo en complemento a 2 de 4 bits)
    clr FLAG_N
    sbrc R_F, 3
    ldi FLAG_N, 1

    ; Flag Z: 1 si F es igual a 0000
    clr FLAG_Z
    tst R_F
    brne UPDATE_OUTPUT
    ldi FLAG_Z, 1


; ACTUALIZACIÓN DE SALIDA EN PORTD

UPDATE_OUTPUT:
    ; Cargar los 4 bits del resultado F (PD0..PD3)
    mov R_OUT, R_F

    ; Cargar Flag C en PD4
    sbrc FLAG_C, 0
    sbr R_OUT, (1<<4)

    ; Cargar Flag N en PD5
    sbrc FLAG_N, 0
    sbr R_OUT, (1<<5)

    ; Cargar Flag Z en PD6
    sbrc FLAG_Z, 0
    sbr R_OUT, (1<<6)

    ; Encender PD7 (Indicador de Operación Activa)
    sbr R_OUT, (1<<7)

    ; Enviar byte consolidado al Puerto D
    out PORTD, R_OUT

    rjmp MAIN_LOOP
