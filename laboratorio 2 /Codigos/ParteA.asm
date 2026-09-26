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

    ldi r16, 0x3F        
    out DDRB, r16
    ldi r16, 0x03       
    out PORTB, r16


    ldi r16, 0x0F     
    out DDRC, r16
    clr r16
    out PORTC, r16     

    rcall INIT_UART

    ldi YL, LOW(FRAME_BUF)
    ldi YH, HIGH(FRAME_BUF)
    clr r16
    ldi r17, 8
CLR_RAM:
    st Y+, r16
    dec r17
    brne CLR_RAM
