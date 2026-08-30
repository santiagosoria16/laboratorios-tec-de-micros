.include "m328pdef.inc" 

.org 0x0000  
    rjmp START   

START:

    ldi r16, LOW(RAMEND)
    out SPL, r16
    ldi r16, HIGH(RAMEND)
    out SPH, r16


    ldi r16, 0xFF
    out DDRD, r16 
    ldi r16, 0x00
    out DDRB, r16   
    ldi r16, 0x07
    out PORTB, r16    

    ldi r20, 1     

MAIN_LOOP:

    cpi r20, 1
    breq RUN_SEQ1
    cpi r20, 2
    breq RUN_SEQ2
    cpi r20, 3
    breq RUN_SEQ3
    cpi r20, 4
    breq RUN_SEQ4
    cpi r20, 5
    breq RUN_SEQ5
    cpi r20, 6
    breq RUN_SEQ6
    cpi r20, 7
    breq RUN_SEQ7
    cpi r20, 8
    breq RUN_SEQ8
    rjmp CHECK_BUTTONS

RUN_SEQ1:
    rcall SEQ_1
    rjmp CHECK_BUTTONS
RUN_SEQ2:
    rcall SEQ_2
    rjmp CHECK_BUTTONS
RUN_SEQ3:
    rcall SEQ_3
    rjmp CHECK_BUTTONS
RUN_SEQ4:
    rcall SEQ_4
    rjmp CHECK_BUTTONS
RUN_SEQ5:
    rcall SEQ_5
    rjmp CHECK_BUTTONS
RUN_SEQ6:
    rcall SEQ_6
    rjmp CHECK_BUTTONS
RUN_SEQ7:
    rcall SEQ_7
    rjmp CHECK_BUTTONS
RUN_SEQ8:
    rcall SEQ_8
    rjmp CHECK_BUTTONS

CHECK_BUTTONS:

    in r17, PINB
    sbrs r17, 0 
    rjmp BTN_INC
    sbrs r17, 1 
    rjmp BTN_DEC
    sbrs r17, 2 
    rjmp BTN_RESET
    rjmp MAIN_LOOP  

; SECUENCIAS LUMINOSAS

; SEQ 1: Auto Fantástico (Barrido de izquierda a derecha)
SEQ_1:
    ldi r16, 0x01
S1_LEFT:
    out PORTD, r16
    rcall DELAY_SEQ
    lsl r16
    brne S1_LEFT
    ldi r16, 0x40
S1_RIGHT:
    out PORTD, r16
    rcall DELAY_SEQ
    lsr r16
    cpi r16, 0x01
    brne S1_RIGHT
    ret

; SEQ 2: Sirena Intercalada (Pares e Impares)
SEQ_2:
    ldi r16, 0xAA
    out PORTD, r16
    rcall DELAY_SEQ
    com r16
    out PORTD, r16
    rcall DELAY_SEQ
    ret

; SEQ 3: Carga de Barra (Llenado progresivo)
SEQ_3:
    clr r16
S3_LOOP:
    out PORTD, r16
    rcall DELAY_SEQ
    lsl r16
    ori r16, 0x01
    cpi r16, 0xFF
    brne S3_LOOP
    out PORTD, r16
    rcall DELAY_SEQ
    ret
