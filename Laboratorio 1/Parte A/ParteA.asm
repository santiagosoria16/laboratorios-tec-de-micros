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

