; ==============================================================================
; PROYECTO: CONTROL DE PLOTTER MEDIANTE ATMEGA328P Y USART
; ASIGNATURA: Tecnologías de Microprocesamiento - UTEC
; DESCRIPCIÓN: Círculo perfecto Minecraft (30x30), Pokémon continuo (Shellder),
;              Casita, Triángulo, Pentagrama.
;              Organización en 2 FILAS con desplazamiento exacto de 20s a la izquierda.
; ==============================================================================

.include "m328pdef.inc"

; ------------------------------------------------------------------------------
; DEFINICIÓN DE REGISTROS
; ------------------------------------------------------------------------------
.def R_TEMP   = r16    ; Registro temporal de propósito general
.def R_DATA   = r17    ; Registro de datos para recepción y transmisión USART
.def R_DELAY1 = r18    ; Contador Nivel 1 para subrutinas de retardo
.def R_DELAY2 = r19    ; Contador Nivel 2 para subrutinas de retardo
.def R_DELAY3 = r20    ; Contador Nivel 3 para subrutinas de retardo
.def R_LOOP   = r21    ; Contador de repeticiones de pasos
