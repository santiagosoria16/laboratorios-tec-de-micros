.include "m328pdef.inc"

.def temp        = r16
.def temp2       = r17
.def dac_val     = r18
.def sample_idx  = r19
.def tbl_base_l  = r20
.def tbl_base_h  = r21

.org 0x0000
    rjmp RESET
.org OC2Aaddr
    rjmp TIMER2_COMPA_ISR

RESET:
    clr r1
    ldi temp, LOW(RAMEND)
    out SPL, temp
    ldi temp, HIGH(RAMEND)
    out SPH, temp


    in temp, DDRD
    ori temp, 0xFC
    out DDRD, temp

    in temp, DDRB
    ori temp, 0x03
    out DDRB, temp


    ldi temp, 0
    sts UBRR0H, temp
    ldi temp, 103
    sts UBRR0L, temp

    ldi temp, (1<<RXEN0) | (1<<TXEN0)
    sts UCSR0B, temp

    ldi temp, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, temp


    ldi temp, (1<<WGM21)
    sts TCCR2A, temp

    ldi temp, (1<<CS21)
    sts TCCR2B, temp

    ldi temp, 100
    sts OCR2A, temp

    ldi temp, (1<<OCIE2A)
    sts TIMSK2, temp


    ldi tbl_base_l, LOW(signal_1 * 2)
    ldi tbl_base_h, HIGH(signal_1 * 2)
    clr sample_idx

    sei
    rcall USART_SendMenu

