; ==============================================================================
; PROBLEMA E: AUTOMATIZACION DE PUERTA DE GARAJE
; Microcontrolador: ATmega328P @ 16 MHz
; ==============================================================================

.include "m328pdef.inc"

; --- Definición de Registros ---
.def STATE        = r20    ; Estado actual de la FSM
.def OBS_FLAG     = r22    ; Flag para señalizar evento de obstáculo desde la ISR
.def TEMP         = r16
.def TEMP2        = r17

; --- Constantes de Estados FSM ---
.equ ST_CERRADA      = 0
.equ ST_ABRIENDO     = 1
.equ ST_ABIERTA      = 2
.equ ST_CERRANDO     = 3
.equ ST_INT_ABRIENDO = 4
.equ ST_INT_CERRANDO = 5

; ==============================================================================
; VECTOR DE INTERRUPCIONES
; ==============================================================================
.org 0x0000
    rjmp RESET
.org 0x0006              ; Vector PCINT0 (Pin Change Interrupt 0)
    rjmp ISR_PCINT0

; ==============================================================================
; INICIALIZACION DEL SISTEMA
; ==============================================================================
RESET:
    ; 1. Inicializar el Stack Pointer
    ldi TEMP, HIGH(RAMEND)
    out SPH, TEMP
    ldi TEMP, LOW(RAMEND)
    out SPL, TEMP

    ; 2. Configuración de E/S
    ; PORTB: PB0 Entrada con Pull-up (Sensor Obstáculo S3)
    cbi DDRB, 0
    sbi PORTB, 0

    ; PORTC: PC0 (S1), PC1 (S2), PC2 (Control_AB), PC3 (Control_CE) como Entradas
    clr TEMP
    out DDRC, TEMP
    ldi TEMP, 0x0F       ; Activar Pull-ups en PC0-PC3
    out PORTC, TEMP

    ; PORTD: PD4 (Motor_AB), PD5 (Motor_CE), PD6 (Alarma), PD1 (TXD) como Salidas
    ldi TEMP, (1<<PD4) | (1<<PD5) | (1<<PD6) | (1<<PD1)
    out DDRD, TEMP
    clr TEMP
    out PORTD, TEMP      ; Motores y Alarma apagados al inicio

    ; 3. Configurar Interrupción PCINT0 (PB0)
    ldi TEMP, (1<<PCIE0)
    sts PCICR, TEMP      ; Habilitar banco PCIE0
    ldi TEMP, (1<<PCINT0)
    sts PCMSK0, TEMP     ; Habilitar máscara para PB0

    ; 4. Configuración del módulo USART (9600 baudios @ 16 MHz, UBRR = 103)
    ldi TEMP, 0
    sts UBRR0H, TEMP
    ldi TEMP, 103
    sts UBRR0L, TEMP
    ldi TEMP, (1<<TXEN0)
    sts UCSR0B, TEMP     ; Habilitar solo Transmisor TX
    ldi TEMP, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, TEMP     ; Formato: 8 bits de datos, 1 bit de parada

    ; 5. Estado Inicial
    ldi STATE, ST_CERRADA
    clr OBS_FLAG
    sei                  ; Habilitar interrupciones globales

    ; Transmitir mensaje inicial
    ldi ZL, LOW(MSG_CERRADA << 1)
    ldi ZH, HIGH(MSG_CERRADA << 1)
    rcall PRINT_STRING

; ==============================================================================
; BUCLE PRINCIPAL (MAIN LOOP & DISPATCHER FSM)
; ==============================================================================
MAIN_LOOP:
    ; Verificar si la ISR detectó un obstáculo
    tst OBS_FLAG
    breq CHECK_FSM
    clr OBS_FLAG
    
    ; Transmitir mensaje de advertencia por seguridad
    ldi ZL, LOW(MSG_OBSTACULO << 1)
    ldi ZH, HIGH(MSG_OBSTACULO << 1)
    rcall PRINT_STRING

CHECK_FSM:
    cpi STATE, ST_CERRADA
    breq STATE_CERRADA
    cpi STATE, ST_ABRIENDO
    breq STATE_ABRIENDO
    cpi STATE, ST_ABIERTA
    breq STATE_ABIERTA
    cpi STATE, ST_CERRANDO
    breq STATE_CERRANDO
    cpi STATE, ST_INT_ABRIENDO
    breq STATE_INT_ABRIENDO
    cpi STATE, ST_INT_CERRANDO
    breq STATE_INT_CERRANDO

    rjmp MAIN_LOOP

; ==============================================================================
; MANEJO DE ESTADOS
; ==============================================================================

; --- Estado 0: Puerta Cerrada ---
STATE_CERRADA:
    sbic PINC, 2         ; Evaluar Control_AB (PC2). Si es 0 (pulsado), continua
    rjmp MAIN_LOOP

    ; Transición -> ST_ABRIENDO
    ldi STATE, ST_ABRIENDO
    sbi PORTD, PD4       ; Motor Abrir ON
    sbi PORTD, PD6       ; Alarma ON

    ldi ZL, LOW(MSG_ABRIENDO << 1)
    ldi ZH, HIGH(MSG_ABRIENDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

; --- Estado 1: Puerta Abriéndose ---
STATE_ABRIENDO:
    sbic PINC, 0         ; Evaluar Sensor S1 (PC0, Puerta Abierta).
    rjmp MAIN_LOOP

    ; Transición -> ST_ABIERTA
    ldi STATE, ST_ABIERTA
    cbi PORTD, PD4       ; Motor Abrir OFF
    cbi PORTD, PD6       ; Alarma OFF

    ldi ZL, LOW(MSG_ABIERTA << 1)
    ldi ZH, HIGH(MSG_ABIERTA << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

; --- Estado 2: Puerta Abierta ---
STATE_ABIERTA:
    sbic PINC, 3         ; Evaluar Control_CE (PC3). Si es 0 (pulsado), continua
    rjmp MAIN_LOOP

    ; Transición -> ST_CERRANDO
    ldi STATE, ST_CERRANDO
    sbi PORTD, PD5       ; Motor Cerrar ON
    sbi PORTD, PD6       ; Alarma ON

    ldi ZL, LOW(MSG_CERRANDO << 1)
    ldi ZH, HIGH(MSG_CERRANDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

; --- Estado 3: Puerta Cerrándose ---
STATE_CERRANDO:
    sbic PINC, 1         ; Evaluar Sensor S2 (PC1, Puerta Cerrada).
    rjmp MAIN_LOOP

    ; Transición -> ST_CERRADA
    ldi STATE, ST_CERRADA
    cbi PORTD, PD5       ; Motor Cerrar OFF
    cbi PORTD, PD6       ; Alarma OFF

    ldi ZL, LOW(MSG_CERRADA << 1)
    ldi ZH, HIGH(MSG_CERRADA << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

; --- Estado 4: Abriendo Interrumpido ---
STATE_INT_ABRIENDO:
    sbic PINC, 2         ; Esperar a pulsar Control_AB (PC2) para reanudar
    rjmp MAIN_LOOP

    ; Reanudar -> ST_ABRIENDO
    ldi STATE, ST_ABRIENDO
    sbi PORTD, PD4       ; Motor Abrir ON
    sbi PORTD, PD6       ; Alarma ON

    ldi ZL, LOW(MSG_ABRIENDO << 1)
    ldi ZH, HIGH(MSG_ABRIENDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

; --- Estado 5: Cerrando Interrumpido ---
STATE_INT_CERRANDO:
    sbic PINC, 3         ; Esperar a pulsar Control_CE (PC3) para reanudar
    rjmp MAIN_LOOP

    ; Reanudar -> ST_CERRANDO
    ldi STATE, ST_CERRANDO
    sbi PORTD, PD5       ; Motor Cerrar ON
    sbi PORTD, PD6       ; Alarma ON

    ldi ZL, LOW(MSG_CERRANDO << 1)
    ldi ZH, HIGH(MSG_CERRANDO << 1)
    rcall PRINT_STRING
    rjmp MAIN_LOOP

; ==============================================================================
; RUTINA DE SERVICIO DE INTERRUPCION (PCINT0) - Detección de Obstáculo (S3)
; ==============================================================================
ISR_PCINT0:
    push TEMP
    in TEMP, SREG
    push TEMP

    ; Comprobar si el pin PB0 pasó a bajo (obstáculo presente)
    sbic PINB, 0
    rjmp EXIT_ISR        ; Si es 1 (liberado), no hace nada

    ; Si está abriendo -> Interrumpir
    cpi STATE, ST_ABRIENDO
    breq INT_DESDE_ABRIENDO

    ; Si está cerrando -> Interrumpir
    cpi STATE, ST_CERRANDO
    breq INT_DESDE_CERRANDO
    rjmp EXIT_ISR

INT_DESDE_ABRIENDO:
    ldi STATE, ST_INT_ABRIENDO
    rjmp DETENER_TODO

INT_DESDE_CERRANDO:
    ldi STATE, ST_INT_CERRANDO

DETENER_TODO:
    cbi PORTD, PD4       ; Apagar Motor Abrir
    cbi PORTD, PD5       ; Apagar Motor Cerrar
    cbi PORTD, PD6       ; Apagar Alarma
    ldi OBS_FLAG, 1      ; Indicar al Main Loop que envíe el mensaje USART

EXIT_ISR:
    pop TEMP
    out SREG, TEMP
    pop TEMP
    reti

; ==============================================================================
; RUTINAS AUXILIARES USART
; ==============================================================================
PRINT_STRING:
    lpm TEMP, Z+
    tst TEMP
    breq END_PRINT
SEND_WAIT:
    lds TEMP2, UCSR0A
    sbrs TEMP2, UDRE0
    rjmp SEND_WAIT
    sts UDR0, TEMP
    rjmp PRINT_STRING
END_PRINT:
    ret

; ==============================================================================
; TABLA DE MENSAJES (Alineados a pares de bytes)
; ==============================================================================
MSG_CERRADA:   .db "Puerta cerrada.", 13, 10, 0        ; 18 bytes (Par)
MSG_ABRIENDO:  .db "Puerta abriendo.", 13, 10, 0, 0     ; 20 bytes (Par)
MSG_ABIERTA:   .db "Puerta abierta.", 13, 10, 0        ; 18 bytes (Par)
MSG_CERRANDO:  .db "Puerta cerrando.", 13, 10, 0, 0     ; 20 bytes (Par)
MSG_OBSTACULO: .db "Obstaculo detectado. Movimiento detenido por seguridad.", 13, 10, 0 ; 58 bytes (Par)
