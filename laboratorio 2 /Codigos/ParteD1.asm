; Parte D --> Emisor

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


    cbi DDRB, DDB0
    cbi DDRB, DDB1
    cbi DDRB, DDB2

    sbi PORTB, PORTB0
    sbi PORTB, PORTB1
    sbi PORTB, PORTB2


    ldi r16, HIGH(UBRR_VAL)
    sts UBRR0H, r16
    ldi r16, LOW(UBRR_VAL)
    sts UBRR0L, r16


    ldi r16, (1<<TXEN0)
    sts UCSR0B, r16

    ldi r16, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, r16

main_loop:

    in r16, PINB
    andi r16, 0x07

    com r16             
    andi r16, 0x07

    rcall usart_transmit

    rcall delay_ms

    rjmp main_loop

; --------------------------------------------------------------------
; Subrutina: Transmision USART
; --------------------------------------------------------------------
usart_transmit:
    lds r17, UCSR0A
    sbrs r17, UDRE0
    rjmp usart_transmit

    sts UDR0, r16
    ret

