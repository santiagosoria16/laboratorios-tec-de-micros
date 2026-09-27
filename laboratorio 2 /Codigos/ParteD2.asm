; Parte D --> Receptor

.include "m328pdef.inc"

.equ F_CPU = 16000000
.equ BAUD = 9600
.equ UBRR_VAL = (F_CPU/(16*BAUD)) - 1

.org 0x0000
    rjmp reset

reset:

    ldi r16, HIGH(RAMEND)
    out SPH, r16
    ldi r16, LOW(RAMEND)
    out SPL, r16

    ldi r16, 0x3F
    out DDRB, r16
    clr r16
    out PORTB, r16

    ldi r16, 0x03
    out DDRC, r16
    clr r16
    out PORTC, r16

    ldi r16, HIGH(UBRR_VAL)
    sts UBRR0H, r16
    ldi r16, LOW(UBRR_VAL)
    sts UBRR0L, r16

    ldi r16, (1<<RXEN0)
    sts UCSR0B, r16

    ldi r16, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, r16

