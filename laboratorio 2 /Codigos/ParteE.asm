.include "m328pdef.inc"

.def STATE        = r20   
.def OBS_FLAG     = r22  
.def TEMP         = r16
.def TEMP2        = r17

.equ ST_CERRADA      = 0
.equ ST_ABRIENDO     = 1
.equ ST_ABIERTA      = 2
.equ ST_CERRANDO     = 3
.equ ST_INT_ABRIENDO = 4
.equ ST_INT_CERRANDO = 5

.org 0x0000
    rjmp RESET
.org 0x0006             
    rjmp ISR_PCINT0

RESET:

    ldi TEMP, HIGH(RAMEND)
    out SPH, TEMP
    ldi TEMP, LOW(RAMEND)
    out SPL, TEMP

    cbi DDRB, 0
    sbi PORTB, 0
