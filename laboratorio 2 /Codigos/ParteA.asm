.include "m328pdef.inc"

.equ FRAME_BUF = 0x0100

.org 0x0000
    rjmp RESET

RESET:

    ldi r16, LOW(RAMEND)
    out SPL, r16
    ldi r16, HIGH(RAMEND)
    out SPH, r16

    ldi r16, 0xFE        
    out DDRD, r16
    ldi r16, 0xFC        
    out PORTD, r16

