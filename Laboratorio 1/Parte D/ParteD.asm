.include "m328pdef.inc"

.def R_A      = r16  
.def R_B      = r17 
.def R_F      = r18  
.def FLAG_C   = r19 
.def FLAG_N   = r20 
.def FLAG_Z   = r21 
.def R_SEL    = r22  
.def R_OUT    = r23  
.def R_TEMP1  = r24  
.def R_TEMP2  = r25 

.org 0x0000
    rjmp SETUP

SETUP:

    ldi R_TEMP1, LOW(RAMEND)
    out SPL, R_TEMP1
    ldi R_TEMP1, HIGH(RAMEND)
    out SPH, R_TEMP1
