.include "m328pdef.inc"     

.org 0x0000                 
    rjmp START            


TABLA_7SEG:
    .db 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F  

START:
    
    ldi r16, LOW(RAMEND)    
    out SPL, r16            
    ldi r16, HIGH(RAMEND)  
    out SPH, r16            

    ldi r16, 0xFC           
    out DDRD, r16           

    ldi r16, 0x08           
    out DDRB, r16           
    ldi r16, 0x07           
    out PORTB, r16          

    clr r20                 

MAIN_LOOP:
   
    ldi ZH, high(TABLA_7SEG * 2) 
    ldi ZL, low(TABLA_7SEG * 2)  
    add ZL, r20             
    clr r16                 
    adc ZH, r16             
    lpm r16, Z            
     
    sbrc r16, 6
    sbi PORTB, 3
    sbrs r16, 6
    cbi PORTB, 3

    lsl r16                 
    lsl r16                 
    out PORTD, r16    

    in r17, PINB            

    sbrs r17, 0             
    rjmp BTN_INC            

    sbrs r17, 1   
    rjmp BTN_DEC            

    sbrs r17, 2             
    rjmp BTN_RESET          

    rjmp MAIN_LOOP  

BTN_INC:
    rcall DELAY_DEBOUNCE    
    cpi r20, 9              
    breq WAIT_RELEASE       
    inc r20                 
    rjmp WAIT_RELEASE  
