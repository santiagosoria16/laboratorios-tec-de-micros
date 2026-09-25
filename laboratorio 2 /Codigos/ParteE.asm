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


    ldi STATE, ST_CERRADA
    clr OBS_FLAG
    sei 


    ldi ZL, LOW(MSG_CERRADA << 1)
    ldi ZH, HIGH(MSG_CERRADA << 1)
    rcall PRINT_STRING


MAIN_LOOP:

    tst OBS_FLAG
    breq CHECK_FSM
    clr OBS_FLAG

    ldi ZL, LOW(MSG_OBSTACULO << 1)
    ldi ZH, HIGH(MSG_OBSTACULO << 1)
    rcall PRINT_STRING
    ldi TEMP, (1<<TXEN0)
    sts UCSR0B, TEMP     
    ldi TEMP, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, TEMP   

CHECK_FSM:
    cpi STATE, ST_CERRADA
    breq STATE_CERRADA
    cpi STATE, ST_ABRIENDO
    breq STATE_ABRIENDO
    cpi STATE, ST_ABIERTA
    breq STATE_ABIERTA
    cpi STATE, ST_CERRANDO
    breq STATE_CERRANDO
    cpi STATE, ST_INT_ABRIENDO
    breq STATE_INT_ABRIENDO
    cpi STATE, ST_INT_CERRANDO
    breq STATE_INT_CERRANDO

    rjmp MAIN_LOOP


STATE_CERRADA:
    sbic PINC, 2 
    rjmp MAIN_LOOP


    ldi STATE, ST_ABRIENDO
    sbi PORTD, PD4      
    sbi PORTD, PD6     

    ldi ZL, LOW(MSG_ABRIENDO << 1)
    ldi ZH, HIGH(MSG_ABRIENDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP


STATE_ABRIENDO:
    sbic PINC, 0        
    rjmp MAIN_LOOP


    ldi STATE, ST_ABIERTA
    cbi PORTD, PD4       
    cbi PORTD, PD6      

    ldi ZL, LOW(MSG_ABIERTA << 1)
    ldi ZH, HIGH(MSG_ABIERTA << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

STATE_ABIERTA:
    sbic PINC, 3         
    rjmp MAIN_LOOP

    ldi STATE, ST_CERRANDO
    sbi PORTD, PD5       
    sbi PORTD, PD6      

    ldi ZL, LOW(MSG_CERRANDO << 1)
    ldi ZH, HIGH(MSG_CERRANDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP


STATE_CERRANDO:
    sbic PINC, 1        
    rjmp MAIN_LOOP

  
    ldi STATE, ST_CERRADA
    cbi PORTD, PD5     
    cbi PORTD, PD6   

    ldi ZL, LOW(MSG_CERRADA << 1)
    ldi ZH, HIGH(MSG_CERRADA << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP


STATE_INT_ABRIENDO:
    sbic PINC, 2        
    rjmp MAIN_LOOP


    ldi STATE, ST_ABRIENDO
    sbi PORTD, PD4       
    sbi PORTD, PD6     

    ldi ZL, LOW(MSG_ABRIENDO << 1)
    ldi ZH, HIGH(MSG_ABRIENDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP


STATE_INT_CERRANDO:
    sbic PINC, 3       
    rjmp MAIN_LOOP

 
    ldi STATE, ST_CERRANDO
    sbi PORTD, PD5     
    sbi PORTD, PD6    

    ldi ZL, LOW(MSG_CERRANDO << 1)
    ldi ZH, HIGH(MSG_CERRANDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

ISR_PCINT0:
    push TEMP
    in TEMP, SREG
    push TEMP

    sbic PINB, 0
    rjmp EXIT_ISR      

    cpi STATE, ST_ABRIENDO
    breq INT_DESDE_ABRIENDO

    cpi STATE, ST_CERRANDO
    breq INT_DESDE_CERRANDO
    rjmp EXIT_ISR

INT_DESDE_ABRIENDO:
    ldi STATE, ST_INT_ABRIENDO
    rjmp DETENER_TODO

INT_DESDE_CERRANDO:
    ldi STATE, ST_INT_CERRANDO

DETENER_TODO:
    cbi PORTD, PD4     
    cbi PORTD, PD5       
    cbi PORTD, PD6      
    ldi OBS_FLAG, 1  

EXIT_ISR:
    pop TEMP
    out SREG, TEMP
    pop TEMP
    reti
