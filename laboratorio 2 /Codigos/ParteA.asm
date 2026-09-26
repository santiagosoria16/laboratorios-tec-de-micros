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

    rcall DELAY_SHORT
    rcall READ_UART

    lsl r20
    sbrc r20, 1
    rjmp ROW_LOOP_B

    rjmp MAIN_LOOP

DISPLAY_FRAME:
    push r25

FRAME_REFRESH:
    cpi r19, 0
    brne DISPLAY_ABORT 

    ldi YL, LOW(FRAME_BUF)
    ldi YH, HIGH(FRAME_BUF)

    ldi r20, 0x04

DISP_ROW_D:
    ldi r16, 0xFC
    out PORTD, r16
    in r16, PORTB
    ori r16, 0x03
    out PORTB, r16

    ld r21, Y+

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
    brne DISP_ROW_D

    ldi r20, 0x01

DISP_ROW_B:
    ldi r16, 0xFC
    out PORTD, r16
    in r16, PORTB
    ori r16, 0x03
    out PORTB, r16

    ld r21, Y+

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

    rcall DELAY_SHORT
    rcall READ_UART

    lsl r20
    sbrc r20, 1
    rjmp DISP_ROW_B

    dec r25
    brne FRAME_REFRESH

DISPLAY_ABORT:
    pop r25
    ret

DELAY_SHORT:
    push r24             
    push r25
    ldi r24, 10
D1: ldi r25, 200
D2: dec r25
    brne D2
    dec r24
    brne D1
    pop r25
    pop r24
    ret

INIT_UART:
    ldi r16, 0
    sts UBRR0H, r16
    ldi r16, 103        
    sts UBRR0L, r16
    ldi r16, (1<<RXEN0) | (1<<TXEN0)
    sts UCSR0B, r16
    ldi r16, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, r16
    ret

READ_UART:
    lds r24, UCSR0A
    sbrs r24, RXC0
    ret            

    lds r24, UDR0

    cpi r24, '+'
    breq CMD_SPEED_UP

    cpi r24, '-'
    breq CMD_SPEED_DOWN

    cpi r24, 'm'
    breq TRIGGER_MENU
    cpi r24, 'M'
    breq TRIGGER_MENU

    cpi r24, '0'
    brne CHECK_STATIC_CMD
    clr r19  
    rcall PRINT_ACK
    ret

CMD_SPEED_UP:
    rcall SPEED_UP
    rcall PRINT_ACK
    ret

CMD_SPEED_DOWN:
    rcall SPEED_DOWN
    rcall PRINT_ACK
    ret

CHECK_STATIC_CMD:
    mov r22, r24
    subi r22, '1'  
    cpi r22, 6
    brcc READ_UART_END

    ldi r19, 1  
    mov r17, r22
    rcall PRINT_ACK
    ret

TRIGGER_MENU:
    rcall PRINT_MENU
    ret

READ_UART_END:
    ret

SPEED_UP:
    cpi r18, 5         
    brcs SPEED_UP_END
    subi r18, 5
SPEED_UP_END:
    ret
SPEED_DOWN:
    cpi r18, 95         
    brcc SPEED_DOWN_END
    ldi r16, 5
    add r18, r16      
SPEED_DOWN_END:
    ret

PRINT_ACK:
    push ZL
    push ZH
    rcall UART_TRANSMIT
    ldi ZL, LOW(ACK_TEXT * 2)
    ldi ZH, HIGH(ACK_TEXT * 2)
    rcall PRINT_STRING_FLASH
    pop ZH
    pop ZL
    ret

PRINT_MENU:
    push ZL
    push ZH
    ldi ZL, LOW(MENU_TEXT * 2)
    ldi ZH, HIGH(MENU_TEXT * 2)
    rcall PRINT_STRING_FLASH
    pop ZH
    pop ZL
    ret

PRINT_STRING_FLASH:
    lpm r24, Z+
    tst r24
    breq PRINT_STRING_END
    rcall UART_TRANSMIT
    rjmp PRINT_STRING_FLASH
PRINT_STRING_END:
    ret

UART_TRANSMIT:
    lds r16, UCSR0A
    sbrs r16, UDRE0
    rjmp UART_TRANSMIT
    sts UDR0, r24
    ret


.align 2
MENU_TEXT:
    .db 0x0D, 0x0A, "==================================", 0x0D, 0x0A
    .db "    MENU MATRIZ DE LEDS 8x8     ", 0x0D, 0x0A
    .db "==================================", 0x0D, 0x0A
    .db "0: Marquesina (CUANTO FALTA... )", 0x0D, 0x0A
    .db "1: Carita Sonriendo ", 0x0D, 0x0A
    .db "2: Carita Guinando  ", 0x0D, 0x0A
    .db "3: Corazon          ", 0x0D, 0x0A
    .db "4: Cara :3          ", 0x0D, 0x0A
    .db "5: Asterisco        ", 0x0D, 0x0A
    .db "6: Cara XD          ", 0x0D, 0x0A
    .db "+ / -: Aumentar / Disminuir Vel.", 0x0D, 0x0A
    .db "==================================", 0x0D, 0x0A
    .db "Seleccione opcion: ", 0x00

ACK_TEXT:
    .db " -> OK!", 0x0D, 0x0A, 0x00

.align 2
TEXT_SCROLL_DATA:
    .db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .db 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, 0x3F, 0x40
    .db 0x40, 0x40, 0x3F, 0x00, 0x7C, 0x12, 0x11, 0x12
    .db 0x7C, 0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00
    .db 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x3E, 0x41
    .db 0x41, 0x41, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x7F
    .db 0x09, 0x09, 0x09, 0x01, 0x00, 0x7C, 0x12, 0x11
    .db 0x12, 0x7C, 0x00, 0x7F, 0x40, 0x40, 0x40, 0x40
    .db 0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x7C
    .db 0x12, 0x11, 0x12, 0x7C, 0x00, 0x00, 0x00, 0x00
    .db 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, 0x7C, 0x12
    .db 0x11, 0x12, 0x7C, 0x00, 0x7F, 0x09, 0x19, 0x29
    .db 0x46, 0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00
    .db 0x00, 0x00, 0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C
    .db 0x00, 0x41, 0x7F, 0x41, 0x00, 0x3E, 0x41, 0x41
    .db 0x41, 0x22, 0x00, 0x41, 0x7F, 0x41, 0x00, 0x7F
    .db 0x49, 0x49, 0x49, 0x41, 0x00, 0x7F, 0x02, 0x0C
    .db 0x02, 0x7F, 0x00, 0x7F, 0x49, 0x49, 0x49, 0x36
    .db 0x00, 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00, 0x7F
    .db 0x49, 0x49, 0x49, 0x41, 0x00, 0x02, 0x01, 0x51
    .db 0x09, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .db 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00


.align 2
PATTERNS:
    .db 0x00, 0xE7, 0xE7, 0xE7, 0x00, 0x81, 0x7E, 0x00
    .db 0x00, 0x07, 0xE7, 0x07, 0x00, 0x81, 0x7E, 0x00
    .db 0x00, 0x6C, 0x92, 0x82, 0x44, 0x28, 0x10, 0x00
    .db 0x00, 0x30, 0x44, 0x60, 0x44, 0x30, 0x00, 0x00
    .db 0x00, 0x10, 0x54, 0x38, 0xFE, 0x38, 0x54, 0x10
    .db 0x00, 0x00, 0x69, 0xA6, 0xA6, 0x69, 0x00, 0x00
