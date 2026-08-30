.include "m328pdef.inc"

.def temp     = r16
.def estado   = r17
.def carga    = r18    
.def timer_cnt= r19

.equ E_STANDBY     
.equ E_LAVADO      
.equ E_CENTRIFUGADO
.equ E_SECADO      
.equ E_FIN         

.org 0x0000
 rjmp SETUP

SETUP:
    ldi temp, HIGH(RAMEND)
    out SPH, temp
    ldi temp, LOW(RAMEND)
    out SPL, temp
  
    ldi temp, 0x38        
    out DDRB, temp
    ldi temp, 0x07          
    out PORTB, temp

    ldi temp, 0x07         
    out DDRC, temp
    ldi temp, (1<<PC5)      
    out PORTC, temp

    ldi temp, 0x7C        
    out DDRD, temp
    ldi temp, 0x00
    out PORTD, temp

    ldi estado, E_STANDBY
    ldi carga, 0  


MAIN_LOOP:
    cpi estado, E_STANDBY
    brne CHECK_LAVADO
    rjmp ST_STANDBY

CHECK_LAVADO:
    cpi estado, E_LAVADO
    brne CHECK_CENTRIFUGADO
    rjmp ST_LAVADO

CHECK_CENTRIFUGADO:
    cpi estado, E_CENTRIFUGADO
    brne CHECK_SECADO
    rjmp ST_CENTRIFUGADO
