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

