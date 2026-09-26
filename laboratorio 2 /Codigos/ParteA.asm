.include "m328pdef.inc"

.equ FRAME_BUF = 0x0100

.org 0x0000
    rjmp RESET

RESET:

    ldi r16, LOW(RAMEND)
    out SPL, r16
    ldi r16, HIGH(RAMEND)
    out SPH, r16

    ldi r16, 0xFE        
    out DDRD, r16
    ldi r16, 0xFC        
    out PORTD, r16

    ldi r16, 0x3F        
    out DDRB, r16
    ldi r16, 0x03       
    out PORTB, r16


    ldi r16, 0x0F     
    out DDRC, r16
    clr r16
    out PORTC, r16     

    rcall INIT_UART

    ldi YL, LOW(FRAME_BUF)
    ldi YH, HIGH(FRAME_BUF)
    clr r16
    ldi r17, 8
CLR_RAM:
    st Y+, r16
    dec r17
    brne CLR_RAM

    clr r19      
    clr r17        
    ldi r18, 35       


    rcall PRINT_MENU

MAIN_LOOP:
    cpi r19, 0
    breq DO_MARQUEE      
    rjmp DO_STATIC

DO_MARQUEE:
    ldi ZL, LOW(TEXT_SCROLL_DATA * 2)
    ldi ZH, HIGH(TEXT_SCROLL_DATA * 2)

SCROLL_NEXT_COL:
    cpi r19, 0
    brne MAIN_LOOP    

    lpm r21, Z+        
    cpi r21, 0xFF       
    breq DO_MARQUEE  

    ldi YL, LOW(FRAME_BUF)
    ldi YH, HIGH(FRAME_BUF)
    ldi r16, 8

SHIFT_ROWS_LOOP:
    ld r22, Y
    lsr r22
    sbrc r21, 0
    ori r22, 0x80
    st Y+, r22
    lsr r21
    dec r16
    brne SHIFT_ROWS_LOOP

    mov r25, r18
    rcall DISPLAY_FRAME

    rjmp SCROLL_NEXT_COL

DO_STATIC:
    mov r16, r17
    lsl r16
    lsl r16
    lsl r16             

    ldi ZL, LOW(PATTERNS * 2)
    ldi ZH, HIGH(PATTERNS * 2)
    clr r0
    add ZL, r16
    adc ZH, r0

    ldi r20, 0x04

ROW_LOOP_D:
    ldi r16, 0xFC
    out PORTD, r16
    in r16, PORTB
    ori r16, 0x03
    out PORTB, r16

    lpm r21, Z+

    mov r22, r21
    andi r22, 0x0F
    lsl r22
    lsl r22
    in r16, PORTB
    andi r16, 0x03
    or r16, r22
    out PORTB, r16

    mov r23, r21
    swap r23
    andi r23, 0x0F
    out PORTC, r23

    mov r16, r20
    com r16
    out PORTD, r16

    rcall DELAY_SHORT
    rcall READ_UART

    lsl r20
    brne ROW_LOOP_D

    ldi r20, 0x01

ROW_LOOP_B:
    ldi r16, 0xFC
    out PORTD, r16
    in r16, PORTB
    ori r16, 0x03
    out PORTB, r16

    lpm r21, Z+

    mov r22, r21
    andi r22, 0x0F
    lsl r22
    lsl r22
    mov r16, r20
    com r16
    andi r16, 0x03
    or r16, r22
    out PORTB, r16

    mov r23, r21
    swap r23
    andi r23, 0x0F
    out PORTC, r23
