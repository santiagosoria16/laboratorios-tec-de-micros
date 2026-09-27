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

MAIN_LOOP:
    lds temp, UCSR0A
    sbrs temp, RXC0
    rjmp MAIN_LOOP

    lds temp, UDR0

    cpi temp, '1'
    breq SELECT_SIG1
    cpi temp, '2'
    breq SELECT_SIG17
    cpi temp, '+'
    breq INC_FREQ
    cpi temp, '-'
    breq DEC_FREQ
    cpi temp, 'm'
    breq SHOW_MENU
    cpi temp, 'M'
    breq SHOW_MENU

    rjmp MAIN_LOOP

SHOW_MENU:
    rcall USART_SendMenu
    rjmp MAIN_LOOP

SELECT_SIG1:
    cli
    ldi tbl_base_l, LOW(signal_1 * 2)
    ldi tbl_base_h, HIGH(signal_1 * 2)
    clr sample_idx
    sei
    rcall USART_SendAck1
    rjmp MAIN_LOOP

SELECT_SIG17:
    cli
    ldi tbl_base_l, LOW(signal_17 * 2)
    ldi tbl_base_h, HIGH(signal_17 * 2)
    clr sample_idx
    sei
    rcall USART_SendAck2
    rjmp MAIN_LOOP

INC_FREQ:
    lds temp, OCR2A
    cpi temp, 25
    brlo MAIN_LOOP
    subi temp, 2 
    sts OCR2A, temp
    rcall USART_SendAckInc
    rjmp MAIN_LOOP

DEC_FREQ:
    lds temp, OCR2A
    cpi temp, 250
    brsh MAIN_LOOP
    subi temp, -2
    sts OCR2A, temp
    rcall USART_SendAckDec
    rjmp MAIN_LOOP

TIMER2_COMPA_ISR:
    push temp
    in temp, SREG
    push temp
    push ZL
    push ZH
    push temp2

    mov ZL, tbl_base_l
    mov ZH, tbl_base_h
    add ZL, sample_idx
    adc ZH, r1

    lpm dac_val, Z
    inc sample_idx

    mov temp, dac_val
    lsl temp
    lsl temp
    in temp2, PORTD
    andi temp2, 0x03
    or temp, temp2
    out PORTD, temp

    mov temp, dac_val
    lsr temp
    lsr temp
    lsr temp
    lsr temp
    lsr temp
    lsr temp
    in temp2, PORTB
    andi temp2, 0xFC
    or temp, temp2
    out PORTB, temp

    pop temp2
    pop ZH
    pop ZL
    pop temp
    out SREG, temp
    pop temp
    reti

USART_TxChar:
    lds temp2, UCSR0A
    sbrs temp2, UDRE0
    rjmp USART_TxChar
    sts UDR0, temp
    ret

USART_SendAck1:
    ldi ZL, LOW(msg_ack1 * 2)
    ldi ZH, HIGH(msg_ack1 * 2)
    rjmp USART_SendString

USART_SendAck2:
    ldi ZL, LOW(msg_ack2 * 2)
    ldi ZH, HIGH(msg_ack2 * 2)
    rjmp USART_SendString

USART_SendMenu:
    ldi ZL, LOW(msg_menu * 2)
    ldi ZH, HIGH(msg_menu * 2)
    rjmp USART_SendString

USART_SendAckInc:
    ldi ZL, LOW(msg_ack_inc * 2)
    ldi ZH, HIGH(msg_ack_inc * 2)
    rjmp USART_SendString

USART_SendAckDec:
    ldi ZL, LOW(msg_ack_dec * 2)
    ldi ZH, HIGH(msg_ack_dec * 2)
