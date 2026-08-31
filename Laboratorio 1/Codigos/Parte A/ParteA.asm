.include "m328pdef.inc"

; REGISTROS Y CONSTANTES

.def temp     = r16
.def estado   = r17
.def carga    = r18     ; 0=Ligera, 1=Media, 2=Pesada
.def timer_cnt= r19

; Estados FSM
.equ E_STANDBY     = 0
.equ E_LAVADO      = 1
.equ E_CENTRIFUGADO= 2
.equ E_SECADO      = 3
.equ E_FIN         = 4


; VECTOR DE RESET

.org 0x0000
    rjmp SETUP


; CONFIGURACIÓN INICIAL

SETUP:
    ; Configurar Stack Pointer
    ldi temp, HIGH(RAMEND)
    out SPH, temp
    ldi temp, LOW(RAMEND)
    out SPL, temp

    ; Configurar Puerto B: PB0..PB2 Entradas, PB3..PB5 Salidas LEDs Carga
    ldi temp, 0x38          ; 0011 1000
    out DDRB, temp
    ldi temp, 0x07          ; Activar Pull-ups en PB0, PB1, PB2
    out PORTB, temp

    ; Configurar Puerto C: PC0..PC2 Salidas (Motores), PC5 Entrada (Sensor Agua)
    ldi temp, 0x07          ; PC0..PC2 salidas
    out DDRC, temp
    ldi temp, (1<<PC5)      ; Activar Pull-up en PC5 (0x20)
    out PORTC, temp

    ; Configurar Puerto D: PD2..PD6 Salidas (LEDs de Estado)
    ldi temp, 0x7C          ; 0111 1100 (PD0 y PD1 libres)
    out DDRD, temp
    ldi temp, 0x00
    out PORTD, temp

    ; Inicialización
    ldi estado, E_STANDBY
    ldi carga, 0            ; Inicia en carga ligera


; BUCLE PRINCIPAL (FSM ROUTER)

MAIN_LOOP:
    cpi estado, E_STANDBY
    brne CHECK_LAVADO
    rjmp ST_STANDBY

CHECK_LAVADO:
    cpi estado, E_LAVADO
    brne CHECK_CENTRIFUGADO
    rjmp ST_LAVADO

CHECK_CENTRIFUGADO:
    cpi estado, E_CENTRIFUGADO
    brne CHECK_SECADO
    rjmp ST_CENTRIFUGADO

CHECK_SECADO:
    cpi estado, E_SECADO
    brne CHECK_FIN
    rjmp ST_SECADO

CHECK_FIN:
    cpi estado, E_FIN
    brne MAIN_LOOP
    rjmp ST_FIN


; ESTADO 0: STANDBY

ST_STANDBY:
    ; Enciende LED StandBy (PD2)
    ldi temp, (1<<PD2)
    out PORTD, temp

    ; Actualizar LEDs Carga (PB3, PB4, PB5)
    rcall MOSTRAR_LEDS_CARGA

    ; Verificar pulsador btn_carga (PB1)
    sbis PINB, 1            ; Presionado = 0
    rcall CAMBIAR_CARGA

    ; Transición: btn_inicio (PB0) = 0, sensor_puerta (PB2) = 0, sensor_agua (PC5) = 0
    sbic PINB, 0            ; Si no se presiona inicio, repite loop
    rjmp MAIN_LOOP
    sbic PINB, 2            ; Si la puerta está abierta (1), repite loop
    rjmp MAIN_LOOP
    sbic PINC, 5            ; Si no hay agua suficiente (1), repite loop
    rjmp MAIN_LOOP

    ; Si las tres condiciones se cumplen, pasa a Lavado
    ldi estado, E_LAVADO
    rjmp MAIN_LOOP

CAMBIAR_CARGA:
    rcall DELAY_200MS       ; Anti-rebote al presionar
    inc carga
    cpi carga, 3
    brne MOSTRAR_CAMBIO
    ldi carga, 0

MOSTRAR_CAMBIO:
    rcall MOSTRAR_LEDS_CARGA ; Refresca la salida visual de inmediato

WAIT_RELEASE_CARGA:
    sbis PINB, 1            ; Si se soltó el botón (PB1 == 1), salta a salir
    rjmp WAIT_RELEASE_CARGA ; Si sigue presionado, espera

    rcall DELAY_200MS       ; Anti-rebote al soltar
    ret

MOSTRAR_LEDS_CARGA:
    in temp, PORTB
    andi temp, 0x07         ; Preserva entradas PB0..PB2
    cpi carga, 0
    breq L_LIGERA
    cpi carga, 1
    breq L_MEDIA
    rjmp L_PESADA

L_LIGERA:
    ori temp, (1<<PB3)
    rjmp OUT_CARGA
L_MEDIA:
    ori temp, (1<<PB4)
    rjmp OUT_CARGA
L_PESADA:
    ori temp, (1<<PB5)
OUT_CARGA:
    out PORTB, temp
    ret


; ESTADO 1: LAVADO

ST_LAVADO:
    ; Enciende LED Lavado (PD3)
    ldi temp, (1<<PD3)
    out PORTD, temp

    ; 5 ciclos de alternancia ON/OFF
    ldi timer_cnt, 5
CICLO_LAVADO:
    ; Encendido Motor (Giro Derecha PC0)
    ldi temp, (1<<PC0)
    out PORTC, temp
    
    ; Selección de tiempo de encendido según carga
    cpi carga, 0
    breq LAV_15S
    cpi carga, 1
    breq LAV_25S
    
    ; Pesada: 5s ON, 2s OFF (en tiempo rápido 5x)
    rcall DELAY_N_SEGUNDOS
    ldi temp, 0x00
    out PORTC, temp
    rcall DELAY_1S
    rcall DELAY_1S
    rjmp SIGUIENTE_LAVADO

LAV_15S: ; Ligera: 2s ON, 1s OFF
    rcall DELAY_1S
    rcall DELAY_1S
    ldi temp, 0x00
    out PORTC, temp
    rcall DELAY_1S
    rjmp SIGUIENTE_LAVADO

LAV_25S: ; Media: 3s ON, 2s OFF
    rcall DELAY_1S
    rcall DELAY_1S
    rcall DELAY_1S
    ldi temp, 0x00
    out PORTC, temp
    rcall DELAY_1S
    rcall DELAY_1S

SIGUIENTE_LAVADO:
    dec timer_cnt
    brne CICLO_LAVADO

    ldi estado, E_CENTRIFUGADO
    rjmp MAIN_LOOP


; ESTADO 2: CENTRIFUGADO

ST_CENTRIFUGADO:
    ; Enciende LED Centrifugado (PD4)
    ldi temp, (1<<PD4)
    out PORTD, temp

    ; Motor a Max Velocidad (PC2)
    ldi temp, (1<<PC2)
    out PORTC, temp

    ; Tiempos: Ligera = 15s, Media = 18s, Pesada = 21s (escala 1:5)
    cpi carga, 0
    breq CENT_15S
    cpi carga, 1
    breq CENT_18S

    ; Pesada 21s
    ldi timer_cnt, 21
    rcall DELAY_MULTI_SEC
    rjmp FIN_CENTRIFUGADO

CENT_15S:
    ldi timer_cnt, 15
    rcall DELAY_MULTI_SEC
    rjmp FIN_CENTRIFUGADO

CENT_18S:
    ldi timer_cnt, 18
    rcall DELAY_MULTI_SEC

FIN_CENTRIFUGADO:
    ldi temp, 0x00
    out PORTC, temp         ; Apaga motor
    ldi estado, E_SECADO
    rjmp MAIN_LOOP


; ESTADO 3: SECADO

ST_SECADO:
    ; Enciende LED Secado (PD5)
    ldi temp, (1<<PD5)
    out PORTD, temp

    ; Secuencia: Derecha 5s, Pausa 3s, Izquierda 5s (escala 1:5)
    ; Giro Derecha (PC0)
    ldi temp, (1<<PC0)
    out PORTC, temp
    ldi timer_cnt, 5
    rcall DELAY_MULTI_SEC

    ; Pausa
    ldi temp, 0x00
    out PORTC, temp
    ldi timer_cnt, 3
    rcall DELAY_MULTI_SEC

    ; Giro Izquierda (PC1)
    ldi temp, (1<<PC1)
    out PORTC, temp
    ldi timer_cnt, 5
    rcall DELAY_MULTI_SEC

    ldi temp, 0x00
    out PORTC, temp         ; Apagar motores

    ldi estado, E_FIN
    rjmp MAIN_LOOP


; ESTADO 4: FIN DEL PROCESO

ST_FIN:
    ; Enciende LED Fin (PD6)
    ldi temp, (1<<PD6)
    out PORTD, temp
    ldi temp, 0x00
    out PORTC, temp         ; Motores apagados

    ; Mantiene LED encendido 5 segundos (25 iteraciones rápidas)
    ldi timer_cnt, 25
    rcall DELAY_MULTI_SEC

    ldi estado, E_STANDBY
    rjmp MAIN_LOOP


; SUBRUTINAS DE TIEMPO Y PROTECCIÓN DE PUERTA

DELAY_MULTI_SEC:
    rcall DELAY_1S
    dec timer_cnt
    brne DELAY_MULTI_SEC
    ret

; Genera ~200ms por ciclo (equivale a 1s acelerado 5 veces)
DELAY_1S:
    push r20
    push r21
    push r22
    ldi r20, 16
    ldi r21, 43
    ldi r22, 0
L_1S:
    ; Verificar si la puerta se abrió durante el retardo (PB2 == 1)
    sbic PINB, 2
    rcall PAUSA_PUERTA

    dec r22
    brne L_1S
    dec r21
    brne L_1S
    dec r20
    brne L_1S
    pop r22
    pop r21
    pop r20
    ret

; Subrutina de Pausa por Puerta Abierta
PAUSA_PUERTA:
    push temp
    in temp, PORTC          ; Guarda el estado previo de los motores
    push temp
    
    ldi temp, 0x00
    out PORTC, temp         ; Apaga motores inmediatamente por seguridad

WAIT_PUERTA:
    sbic PINB, 2            ; ¿La puerta sigue abierta? (PB2 == 1)
    rjmp WAIT_PUERTA        ; Queda congelado en bucle hasta cerrar

    rcall DELAY_200MS       ; Anti-rebote al cerrar la puerta

    pop temp
    out PORTC, temp         ; Restaura los motores exactamente a su estado previo
    pop temp
    ret
	
; Genera ~40ms para antirrebote
DELAY_200MS:
    push r20
    push r21
    ldi r20, 20
    ldi r21, 0
L_200MS:
    dec r21
    brne L_200MS
    dec r20
    brne L_200MS
    pop r21
    pop r20
    ret

; Retardo de 5 segundos acelerados (Protege timer_cnt con PUSH/POP)
DELAY_N_SEGUNDOS:
    push timer_cnt
    ldi timer_cnt, 5
L_N_SEC:
    rcall DELAY_1S
    dec timer_cnt
    brne L_N_SEC
    pop timer_cnt
    ret
