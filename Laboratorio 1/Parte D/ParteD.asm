.include "m328pdef.inc"

.def R_A      = r16  
.def R_B      = r17 
.def R_F      = r18  
.def FLAG_C   = r19 
.def FLAG_N   = r20 
.def FLAG_Z   = r21 
.def R_SEL    = r22  
.def R_OUT    = r23  
.def R_TEMP1  = r24  
.def R_TEMP2  = r25 

.org 0x0000
    rjmp SETUP

SETUP:

    ldi R_TEMP1, LOW(RAMEND)
    out SPL, R_TEMP1
    ldi R_TEMP1, HIGH(RAMEND)
    out SPH, R_TEMP1

    ldi R_TEMP1, 0xFF
    out DDRD, R_TEMP1
    clr R_TEMP1
    out PORTD, R_TEMP1

    clr R_TEMP1
    out DDRC, R_TEMP1

    clr R_TEMP1
    out DDRB, R_TEMP1

MAIN_LOOP:

    in R_TEMP1, PINC

    mov R_A, R_TEMP1
    andi R_A, 0x0F

    mov R_SEL, R_TEMP1
    lsr R_SEL
    lsr R_SEL
    lsr R_SEL
    lsr R_SEL
    andi R_SEL, 0x03   

    in R_TEMP2, PINB

    mov R_B, R_TEMP2
    andi R_B, 0x0F

    sbrc R_TEMP2, 4
    ori R_SEL, 0x04     ; R_SEL = [0000 0 S2 S1 S0] (Valor 0 a 7)


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

EXEC_CLEAR:
    clr R_F
    clr FLAG_C
    rjmp CALC_FLAGS

EXEC_SUB:
    mov R_F, R_A
    sub R_F, R_B
    clr FLAG_C
    brcc SUB_NO_CARRY
    ldi FLAG_C, 1      

SUB_NO_CARRY:
    andi R_F, 0x0F
    rjmp CALC_FLAGS


EXEC_ADD:
    mov R_F, R_A
    add R_F, R_B
    clr FLAG_C
    cpi R_F, 16
    brlo ADD_NO_CARRY
    ldi FLAG_C, 1       
ADD_NO_CARRY:
    andi R_F, 0x0F
    rjmp CALC_FLAGS


EXEC_XOR:
    mov R_F, R_A
    eor R_F, R_B
    andi R_F, 0x0F
    clr FLAG_C
    rjmp CALC_FLAGS

EXEC_AND:
    mov R_F, R_A
    and R_F, R_B
    andi R_F, 0x0F
    clr FLAG_C
    rjmp CALC_FLAGS

EXEC_OR:
    mov R_F, R_A
    or R_F, R_B
    andi R_F, 0x0F
    clr FLAG_C
    rjmp CALC_FLAGS

EXEC_SHL:
    clr FLAG_C
    sbrc R_A, 3         
    ldi FLAG_C, 1
    mov R_F, R_A
    lsl R_F
    andi R_F, 0x0F
    rjmp CALC_FLAGS

EXEC_INC:
    mov R_F, R_A
    inc R_F
    clr FLAG_C
    cpi R_F, 16
    brlo INC_NO_CARRY
    ldi FLAG_C, 1      
INC_NO_CARRY:
    andi R_F, 0x0F
    rjmp CALC_FLAGS
