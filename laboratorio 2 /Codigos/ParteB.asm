.include "m328pdef.inc"

.def temp        = r16
.def temp2       = r17
.def dac_val     = r18
.def sample_idx  = r19
.def tbl_base_l  = r20
.def tbl_base_h  = r21

.org 0x0000
    rjmp RESET
.org OC2Aaddr
    rjmp TIMER2_COMPA_ISR
