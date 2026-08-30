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
