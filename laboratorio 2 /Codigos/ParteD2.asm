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

main_loop:
    rcall usart_receive
    andi r16, 0x07
    rcall decode_and_display
    rjmp main_loop

; --------------------------------------------------------------------
; Subrutina: Recepcion USART
; --------------------------------------------------------------------
usart_receive:
    lds r17, UCSR0A
    sbrs r17, RXC0
    rjmp usart_receive

    lds r16, UDR0
    ret

; --------------------------------------------------------------------
; Subrutina: Decodificador 1 de 8 (Activa un solo LED)
; --------------------------------------------------------------------
decode_and_display:
    clr r17
    clr r18

    cpi r16, 6
    brsh output_portc

output_portb:
    ldi r17, 1
    tst r16
    breq set_ports

shift_b:
    lsl r17
    dec r16
    brne shift_b
    rjmp set_ports

output_portc:
    subi r16, 6
    ldi r18, 1
    tst r16
    breq set_ports

shift_c:
    lsl r18
    dec r16
    brne shift_c

set_ports:
    out PORTB, r17
    out PORTC, r18
    ret
