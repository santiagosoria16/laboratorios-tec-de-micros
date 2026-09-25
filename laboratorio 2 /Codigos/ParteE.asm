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

    clr TEMP
    out DDRC, TEMP
    ldi TEMP, 0x0F     
    out PORTC, TEMP

    ldi TEMP, (1<<PD4) | (1<<PD5) | (1<<PD6) | (1<<PD1)
    out DDRD, TEMP
    clr TEMP
    out PORTD, TEMP   

 
    ldi TEMP, (1<<PCIE0)
    sts PCICR, TEMP      
    ldi TEMP, (1<<PCINT0)
    sts PCMSK0, TEMP    

    ldi TEMP, 0
    sts UBRR0H, TEMP
    ldi TEMP, 103
    sts UBRR0L, TEMP
    ldi TEMP, (1<<TXEN0)
    sts UCSR0B, TEMP     
    ldi TEMP, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, TEMP   
