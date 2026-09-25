.include "m328pdef.inc"

.def STATE        = r20   
.def OBS_FLAG     = r22  
.def TEMP         = r16
.def TEMP2        = r17

.equ ST_CERRADA      = 0
.equ ST_ABRIENDO     = 1
.equ ST_ABIERTA      = 2
.equ ST_CERRANDO     = 3
.equ ST_INT_ABRIENDO = 4
.equ ST_INT_CERRANDO = 5

