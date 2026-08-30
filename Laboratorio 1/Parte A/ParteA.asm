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

CHECK_SECADO:
    cpi estado, E_SECADO
    brne CHECK_FIN
    rjmp ST_SECADO

CHECK_FIN:
    cpi estado, E_FIN
    brne MAIN_LOOP
    rjmp ST_FIN

ST_STANDBY:
    ; Enciende LED StandBy (PD2)
    ldi temp, (1<<PD2)
    out PORTD, temp

    rcall MOSTRAR_LEDS_CARGA

    sbis PINB, 1           
    rcall CAMBIAR_CARGA

    sbic PINB, 0            
    rjmp MAIN_LOOP
    sbic PINB, 2           
    rjmp MAIN_LOOP
    sbic PINC, 5           
    rjmp MAIN_LOOP

    ldi estado, E_LAVADO
    rjmp MAIN_LOO


    rcall DELAY_200MS      
    inc carga
    cpi carga, 3
    brne MOSTRAR_CAMBIO
    ldi carga, 0


    rcall MOSTRAR_LEDS_CARGA

WAIT_RELEASE_CARGA:
    sbis PINB, 1           
    rjmp WAIT_RELEASE_CARGA 

    rcall DELAY_200MS       
    ret

MOSTRAR_LEDS_CARGA:
    in temp, PORTB
    andi temp, 0x07        
    cpi carga, 0
    breq L_LIGERA
    cpi carga, 1
    breq L_MEDIA
    rjmp L_PESADA

L_LIGERA:
    ori temp, (1<<PB3)
    rjmp OUT_CARGA
L_MEDIA:
    ori temp, (1<<PB4)
    rjmp OUT_CARGA
L_PESADA:
    ori temp, (1<<PB5)
OUT_CARGA:
    out PORTB, temp
    ret
; ESTADO 1: LAVADO

ST_LAVADO:
    ; Enciende LED Lavado (PD3)
    ldi temp, (1<<PD3)
    out PORTD, temp

   
    ldi timer_cnt, 5
CICLO_LAVADO:

    ldi temp, (1<<PC0)
    out PORTC, temp


    cpi carga, 0
    breq LAV_15S
    cpi carga, 1
    breq LAV_25S
    
 
    rcall DELAY_N_SEGUNDOS
    ldi temp, 0x00
    out PORTC, temp
    rcall DELAY_1S
    rcall DELAY_1S
    rjmp SIGUIENTE_LAVADO


LAV_15S: 
    rcall DELAY_1S
    rcall DELAY_1S
    ldi temp, 0x00
    out PORTC, temp
    rcall DELAY_1S
    rjmp SIGUIENTE_LAVADO

LAV_25S: 
    rcall DELAY_1S
    rcall DELAY_1S
    rcall DELAY_1S
    ldi temp, 0x00
    out PORTC, temp
    rcall DELAY_1S
    rcall DELAY_1S

SIGUIENTE_LAVADO:
    dec timer_cnt
    brne CICLO_LAVADO

    ldi estado, E_CENTRIFUGADO
    rjmp MAIN_LOOP

ST_CENTRIFUGADO:
 
    ldi temp, (1<<PD4)
    out PORTD, temp

    ldi temp, (1<<PC2)
    out PORTC, temp

    cpi carga, 0
    breq CENT_15S
    cpi carga, 1
    breq CENT_18S



    ldi timer_cnt, 21
    rcall DELAY_MULTI_SEC
    rjmp FIN_CENTRIFUGADO

CENT_15S:
    ldi timer_cnt, 15
    rcall DELAY_MULTI_SEC
    rjmp FIN_CENTRIFUGADO

CENT_18S:
    ldi timer_cnt, 18
    rcall DELAY_MULTI_SEC

FIN_CENTRIFUGADO:
    ldi temp, 0x00
    out PORTC, temp         
    ldi estado, E_SECADO
    rjmp MAIN_LOOP

ST_SECADO:
   
    ldi temp, (1<<PD5)
    out PORTD, temp

    ldi temp, (1<<PC0)
    out PORTC, temp
    ldi timer_cnt, 5
    rcall DELAY_MULTI_SEC

    ldi temp, 0x00
    out PORTC, temp
    ldi timer_cnt, 3
    rcall DELAY_MULTI_SEC

    ldi temp, (1<<PC1)
    out PORTC, temp
    ldi timer_cnt, 5
    rcall DELAY_MULTI_SEC

    ldi temp, 0x00
    out PORTC, temp        

    ldi estado, E_FIN
    rjmp MAIN_LOOP

ST_FIN:
  
    ldi temp, (1<<PD6)
    out PORTD, temp
    ldi temp, 0x00
    out PORTC, temp        
   
    ldi timer_cnt, 25
    rcall DELAY_MULTI_SEC

    ldi estado, E_STANDBY
    rjmp MAIN_LOOP

DELAY_MULTI_SEC:
    rcall DELAY_1S
    dec timer_cnt
    brne DELAY_MULTI_SEC
    ret


DELAY_1S:
    push r20
    push r21
    push r22
    ldi r20, 16
    ldi r21, 43
    ldi r22, 0
L_1S:
  
    sbic PINB, 2
    rcall PAUSA_PUERTA

    dec r22
    brne L_1S
    dec r21
    brne L_1S
    dec r20
    brne L_1S
    pop r22
    pop r21
    pop r20
    ret


PAUSA_PUERTA:
    push temp
    in temp, PORTC         
    push temp
    
    ldi temp, 0x00
    out PORTC, temp         

WAIT_PUERTA:
    sbic PINB, 2           
    rjmp WAIT_PUERTA       

    rcall DELAY_200MS       

    pop temp
    out PORTC, temp        
    pop temp
    ret



