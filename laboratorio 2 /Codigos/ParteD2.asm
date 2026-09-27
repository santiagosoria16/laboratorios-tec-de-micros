; Parte D --> Receptor

.include "m328pdef.inc"

.equ F_CPU = 16000000
.equ BAUD = 9600
.equ UBRR_VAL = (F_CPU/(16*BAUD)) - 1

.org 0x0000
    rjmp reset

