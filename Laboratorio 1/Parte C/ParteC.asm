.include "m328pdef.inc" 

.org 0x0000  
    rjmp START   

START:

    ldi r16, LOW(RAMEND)
    out SPL, r16
    ldi r16, HIGH(RAMEND)
    out SPH, r16


    ldi r16, 0xFF
    out DDRD, r16           ; PORTD (PD0-PD7) como salidas para los 8 LEDs
    ldi r16, 0x00
    out DDRB, r16           ; PORTB como entradas para botones
    ldi r16, 0x07
    out PORTB, r16          ; Activa Pull-Up internas en PB0, PB1 y PB2

    ldi r20, 1              ; El sistema arranca en la SECUENCIA 1

MAIN_LOOP:
    ; --- CONMUTADOR DE SECUENCIAS (1 a 8) ---
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
    ; --- LECTURA DE BOTONES EN PORTB ---
    in r17, PINB
    sbrs r17, 0             ; ¿Botón Incrementar presionado? (PB0)
    rjmp BTN_INC
    sbrs r17, 1             ; ¿Botón Decrementar presionado? (PB1)
    rjmp BTN_DEC
    sbrs r17, 2             ; ¿Botón Reset presionado? (PB2)
    rjmp BTN_RESET
    rjmp MAIN_LOOP          ; Si nada se presiona, repite la secuencia actual
