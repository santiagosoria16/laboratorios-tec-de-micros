/*
- ROBOT BLUETOOTH - Concurso de Robotica UTEC
- Microcontrolador: ATmega328P @ 16 MHz (Microchip Studio / avr-gcc)
 *
- PROTOCOLO (cada trama termina en '\n'):
-   App -> Robot
-     J,v,w   Joystick. v = avance (-100..100, + adelante)
-                       w = giro   (-100..100, + a la derecha)
-     O       Abrir pinza
-     C       Cerrar pinza
-     G       Guardar pinza (posicion de transporte/guardado)
-     S       Stop (frena y deja el joystick en 0)
-     E       Emergencia: frena y bloquea todo hasta recibir R
-     R       Rearmar (sale de emergencia)
-   Robot -> App (cada 500 ms)
-     T,garra,obstaculo
-        garra     = 1 cerrada, 2 guardada, 0 abierta/en movimiento
-        obstaculo = 1 si el IR detecta algo
 *
- PINES
-   Motor izq : ENA = PD5 (OC0B), IN1 = PD4, IN2 = PB3
-   Motor der : ENB = PD6 (OC0A), IN3 = PD7, IN4 = PB0
-   Servo pinza 1: PB1 (OC1A)    Servo pinza 2: PB2 (OC1B)
-   Sensor IR: PC0
-   HC-05: TX -> PD0 (RXD), RX <- PD1 (TXD) (9600 baud)
-   Timer0 = PWM motores, Timer1 = PWM servos, Timer2 = base de tiempo 1 ms
 */

#define F_CPU 16000000UL         // Frecuencia del reloj del CPU: 16 MHz
#include <avr/io.h>              // Registros de puertos, timers y UART del ATmega328P
#include <avr/interrupt.h>       // Macros para rutinas de interrupcion (ISR, sei, cli)
#include <stdint.h>              // Tipos enteros de ancho fijo (uint8_t, int16_t, etc.)
#include <stdlib.h>              // Funciones abs() y labs()

/* ======================= CONFIGURACION ======================= */
#define BAUD    9600                          // Velocidad del Bluetooth HC-05
#define MY_UBRR (F_CPU / 16 / BAUD - 1)       // Valor del registro baud rate (103 para 9600 @ 16MHz)

/* Limites PWM de los servos (Timer1 con prescaler 8: 2000 cuentas = 1 ms) */
#define SERVO_MIN_OCR      2500  // Pulso para servo 1 abierto 
#define SERVO_MAX_OCR      2100  // Pulso para servo 1 cerrado 
#define SERVO2_ABIERTO_OCR 1999  // Pulso para servo 2 abierto 
#define SERVO2_CERRADO_OCR 2500  // Pulso para servo 2 cerrado 
#define SERVO_PASO    40         // Velocidad de movimiento: 40 cuentas cada 10 ms (~0.5 s total)

/* Angulos de posicion de guardado/transporte */
#define GUARDAR_ANG_SERVO1  100  // Angulo objetivo del servo 1 al guardar (en grados)
#define GUARDAR_ANG_SERVO2  100  // Angulo objetivo del servo 2 al guardar (en grados)

// Macro que convierte grados (0..180) a cuentas del registro OCR1 (1999..3999)
#define ANG_A_OCR(a)  ((uint16_t)(1999UL + ((uint32_t)(a) * 2000UL) / 180UL))
#define SERVO_GUARDAR_OCR   ANG_A_OCR(GUARDAR_ANG_SERVO1)
#define SERVO2_GUARDAR_OCR  ANG_A_OCR(GUARDAR_ANG_SERVO2)

#define ZONA_MUERTA        8     // Ignora valores del joystick menores al 8% (evita deriva)
#define TIMEOUT_ENLACE_MS  500   // Si pasan 500 ms sin recibir datos, detiene el robot por seguridad
#define RAMPA_PASO         20    // Incremento maximo de PWM cada 10 ms (aceleracion suave)
#define FRENO_ACTIVO       1     // 1: cortocircuita bobinas al frenar (IN1=IN2=1); 0: rueda libre

#ifndef INVERTIR_GIRO
#define INVERTIR_GIRO 1          // 1: invierte el signo del giro si los motores quedaron cruzados
#endif

/* Calibracion de motores (zona muerta mecanica y desbalance de potencia) */
#ifndef PWM_MIN_IZQ
#define PWM_MIN_IZQ  50          // PWM minimo para vencer la inercia del motor izquierdo
#endif
#ifndef PWM_MIN_DER
#define PWM_MIN_DER  50          // PWM minimo para vencer la inercia del motor derecho
#endif
#ifndef TRIM_IZQ_ADE
#define TRIM_IZQ_ADE 100         // Factor de potencia (%) motor izq adelante
#endif
#ifndef TRIM_IZQ_ATR
#define TRIM_IZQ_ATR 100         // Factor de potencia (%) motor izq atras
#endif
#ifndef TRIM_DER_ADE
#define TRIM_DER_ADE 65          // Factor de potencia (%) motor der adelante (reducido porque corre mas)
#endif
#ifndef TRIM_DER_ATR
#define TRIM_DER_ATR 100         // Factor de potencia (%) motor der atras
#endif

/* Configuracion del sensor infrarrojo delantero */
#define IR_NINGUNO   0           // Solo reporta obstaculo por telemetria
#define IR_LIMITAR   1           // Reduce velocidad maxima de avance ante obstaculo
#define IR_BLOQUEAR  2           // Impide avanzar (v > 0) ante obstaculo
#define IR_MODO      IR_BLOQUEAR // Modo activo elegido
#define IR_ACTIVO_ALTO 0         // 0: sensor entrega 0V al detectar (tipico modulo FC-51)
#define IR_VEL_MAX   40          // Limite de avance (%) si se usa IR_LIMITAR

#define ACEPTAR_FORMATO_VIEJO 1  // 1: habilita compatibilidad con tramas "x,y" sin letra inicial

/* ======================= ESTADO ======================= */
static volatile uint32_t g_ms = 0;        // Contador global de milisegundos (modificado en ISR)

#define RX_TAM 64                         // Tamano del buffer circular UART (potencia de 2)
static volatile uint8_t rx_buf[RX_TAM];   // Buffer de recepcion serial
static volatile uint8_t rx_head = 0;      // Indice de escritura (ISR)
static volatile uint8_t rx_tail = 0;      // Indice de lectura (bucle principal)

static int16_t  cmd_v = 0, cmd_w = 0;     // Consignas actuales de avance y giro (-100..100)
static int16_t  pwm_izq = 0, pwm_der = 0; // Valores actuales de PWM tras aplicar rampa (-255..255)
static uint32_t ultimo_cmd = 0;           // Marca de tiempo (ms) del ultimo comando valido
static uint8_t  emergencia = 0;           // Bandera de paro de emergencia activo (1 = bloqueado)

/* Variables de control de posicion de la pinza */
static uint16_t ocr1Actual   = SERVO_MIN_OCR;      // Posicion instantanea servo 1
static uint16_t ocr1Objetivo = SERVO_MIN_OCR;      // Posicion destino servo 1
static uint16_t ocr2Actual   = SERVO2_ABIERTO_OCR; // Posicion instantanea servo 2
static uint16_t ocr2Objetivo = SERVO2_ABIERTO_OCR; // Posicion destino servo 2

#define LINEA_TAM 24                      // Longitud maxima de una trama recibida
static char    linea[LINEA_TAM];          // Buffer para ensamblar una linea hasta '\n'
static uint8_t linea_len = 0;             // Caracteres acumulados en la linea actual
static uint8_t linea_desbordada = 0;      // 1 si la trama supero LINEA_TAM (se descartara)

/* ======================= TIEMPO (Timer2, 1 kHz) ======================= */
// Interrupcion cada 1 ms exacto
ISR(TIMER2_COMPA_vect)
{
    g_ms++;
}

// Lee g_ms de forma atomica (deshabilita interrupciones brevemente para evitar lectura corrupta de 32 bits)
static uint32_t millis(void)
{
    uint32_t m;
    uint8_t s = SREG;            // Guarda estado actual de interrupciones
    cli();                       // Deshabilita interrupciones
    m = g_ms;
    SREG = s;                    // Restaura estado de interrupciones
    return m;
}

/* ======================= UART ======================= */
// Interrupcion por cada byte recibido desde el modulo Bluetooth
ISR(USART_RX_vect)
{
    uint8_t estado = UCSR0A;                               // Lee flags de error antes de leer UDR0
    uint8_t dato   = UDR0;                                 // Lee el byte recibido (limpia flag RXC0)
    if (estado & ((1 << FE0) | (1 << DOR0))) return;       // Descarta si hubo error de trama o sobrescritura
    uint8_t sig = (rx_head + 1) & (RX_TAM - 1);            // Calcula siguiente posicion circular
    if (sig != rx_tail) {                                  // Si el buffer no esta lleno:
        rx_buf[rx_head] = dato;                            // Guarda el byte
        rx_head = sig;                                     // Avanza el puntero de escritura
    }
}

// Extrae un byte del buffer circular; devuelve 1 si habia dato, 0 si estaba vacio
static uint8_t rx_leer(uint8_t *c)
{
    if (rx_head == rx_tail) return 0;
    *c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & (RX_TAM - 1);
    return 1;
}

// Envia un caracter por UART esperando a que el registro de transmision este libre
static void tx_char(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = (uint8_t)c;
}

/* ======================= INICIALIZACION ======================= */
static void USART_Init(void)
{
    UBRR0H = (uint8_t)(MY_UBRR >> 8);                      // Byte alto del baud rate
    UBRR0L = (uint8_t)MY_UBRR;                             // Byte bajo del baud rate
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);  // Habilita RX, TX e interrupcion de recepcion
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);                // Trama 8 bits, sin paridad, 1 bit de stop (8N1)
}

static void Timer0_Motors_Init(void)
{
    DDRD |= (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7); // Pines puente H y PWM como salida
    DDRB |= (1 << PB0) | (1 << PB3);                           // Pines direccion puente H como salida
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00); // Fast PWM 8-bit en OC0A y OC0B
    TCCR0B = (1 << CS01) | (1 << CS00);                        // Prescaler 64 -> frecuencia PWM ~976 Hz
    OCR0A = 0;                                                 // Ciclo de trabajo inicial 0% (der)
    OCR0B = 0;                                                 // Ciclo de trabajo inicial 0% (izq)
}

static void Timer1_Servo_Init(void)
{
    DDRB |= (1 << PB1) | (1 << PB2);                           // Pines OC1A y OC1B (servos) como salida
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);     // Fast PWM modo 14 (TOP en ICR1)
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);        // Prescaler 8 (1 tick = 0.5 us)
    ICR1   = 39999;                                            // Periodo de 20 ms (50 Hz estandar servo)
    OCR1A  = SERVO_MIN_OCR;                                    // Posicion inicial servo 1
    OCR1B  = SERVO2_ABIERTO_OCR;                               // Posicion inicial servo 2
}

static void Timer2_Tick_Init(void)
{
    TCCR2A = (1 << WGM21);                                     // Modo CTC (reinicia al llegar a OCR2A)
    TCCR2B = (1 << CS22);                                      // Prescaler 64 (16 MHz / 64 = 250 kHz)
    OCR2A  = 249;                                              // 250 cuentas -> 1000 Hz (1 ms exacto)
    TIMSK2 = (1 << OCIE2A);                                    // Habilita interrupcion por comparacion A
}

static void Sensor_IR_Init(void)
{
    DDRC &= ~(1 << PC0);                                       // Pin PC0 como entrada
#if !IR_ACTIVO_ALTO
    PORTC |= (1 << PC0);                                       // Activa resistencia pull-up interna
#endif
}

// Devuelve 1 si hay obstaculo frente al sensor IR, 0 si esta despejado
static uint8_t ir_obstaculo(void)
{
    uint8_t nivel = (PINC & (1 << PC0)) ? 1 : 0;
#if IR_ACTIVO_ALTO
    return nivel;
#else
    return !nivel;                                             // Invierte logica porque el sensor es activo bajo
#endif
}

/* ======================= TRACCION ======================= */
// Controla sentido y PWM del motor izquierdo (-255..255)
static void motor_izq(int16_t u)
{
    if (u > 0) {                                               // Adelante
        PORTD |= (1 << PD4);  PORTB &= ~(1 << PB3);  OCR0B = (uint8_t)u;
    } else if (u < 0) {                                        // Atras
        PORTD &= ~(1 << PD4); PORTB |= (1 << PB3);   OCR0B = (uint8_t)(-u);
    } else {                                                   // Detenido
#if FRENO_ACTIVO
        PORTD |= (1 << PD4);  PORTB |= (1 << PB3);   OCR0B = 255; // Freno electrico activo
#else
        PORTD &= ~(1 << PD4); PORTB &= ~(1 << PB3);  OCR0B = 0;   // Motor libre
#endif
    }
}

// Controla sentido y PWM del motor derecho (-255..255)
static void motor_der(int16_t u)
{
    if (u > 0) {                                               // Adelante
        PORTD |= (1 << PD7);  PORTB &= ~(1 << PB0);  OCR0A = (uint8_t)u;
    } else if (u < 0) {                                        // Atras
        PORTD &= ~(1 << PD7); PORTB |= (1 << PB0);   OCR0A = (uint8_t)(-u);
    } else {                                                   // Detenido
#if FRENO_ACTIVO
        PORTD |= (1 << PD7);  PORTB |= (1 << PB0);   OCR0A = 255; // Freno electrico activo
#else
        PORTD &= ~(1 << PD7); PORTB &= ~(1 << PB0);  OCR0A = 0;   // Motor libre
#endif
    }
}

// Pone consignas y motores a cero inmediatamente (sin rampa)
static void parada_inmediata(void)
{
    cmd_v = 0;
    cmd_w = 0;
    pwm_izq = 0;
    pwm_der = 0;
    motor_izq(0);
    motor_der(0);
}

// Suaviza la aceleracion; al frenar o invertir sentido responde al instante
static int16_t rampa(int16_t actual, int16_t objetivo)
{
    if (objetivo == 0) return 0;                               // Frenado inmediato
    if ((actual > 0 && objetivo < 0) || (actual < 0 && objetivo > 0)) return 0; // Pasa por 0 al invertir
    if (abs(objetivo) < abs(actual)) return objetivo;          // Desaceleracion inmediata
    if (objetivo > actual) {                                   // Acelerando hacia adelante
        actual += RAMPA_PASO;
        if (actual > objetivo) actual = objetivo;
    } else {                                                   // Acelerando hacia atras
        actual -= RAMPA_PASO;
        if (actual < objetivo) actual = objetivo;
    }
    return actual;
}

// Re-escala el rango [1..255] a [pmin..255] para superar la zona muerta del motor
static int16_t aplicar_minimo(int16_t u, int16_t pmin)
{
    if (u == 0) return 0;
    int16_t m = (int16_t)(pmin + (int32_t)abs(u) * (255 - pmin) / 255);
    return u > 0 ? m : (int16_t)-m;
}

// Calcula la mezcla diferencial y actualiza los motores cada 10 ms
static void actualizar_traccion(void)
{
    int16_t v = cmd_v, w = cmd_w;

    if (abs(v) < ZONA_MUERTA) v = 0;                           // Filtra ruido cerca del centro
    if (abs(w) < ZONA_MUERTA) w = 0;

#if IR_MODO == IR_LIMITAR
    if (v > IR_VEL_MAX && ir_obstaculo()) v = IR_VEL_MAX;      // Topa velocidad si hay obstaculo
#elif IR_MODO == IR_BLOQUEAR
    if (v > 0 && ir_obstaculo()) v = 0;                        // Bloquea avance si hay obstaculo
#endif

#if INVERTIR_GIRO
    w = -w;
#endif
    int32_t izq = (int32_t)v + w;                              // Cinematica diferencial basica
    int32_t der = (int32_t)v - w;
    int32_t m = labs(izq) > labs(der) ? labs(izq) : labs(der);
    if (m > 100) {                                             // Normaliza si excede 100% sin perder proporcion de giro
        izq = izq * 100 / m;
        der = der * 100 / m;
    }

    // Aplica porcentaje de calibracion individual por motor y sentido
    izq = izq * (izq >= 0 ? TRIM_IZQ_ADE : TRIM_IZQ_ATR) / 100;
    der = der * (der >= 0 ? TRIM_DER_ADE : TRIM_DER_ATR) / 100;

    // Convierte porcentaje (-100..100) a PWM (-255..255), aplica rampa y minimo
    pwm_izq = rampa(pwm_izq, (int16_t)(izq * 255 / 100));
    pwm_der = rampa(pwm_der, (int16_t)(der * 255 / 100));
    motor_izq(aplicar_minimo(pwm_izq, PWM_MIN_IZQ));
    motor_der(aplicar_minimo(pwm_der, PWM_MIN_DER));
}

/* ======================= PINZA ======================= */
// Diferencia absoluta entre dos enteros sin signo de 16 bits
static uint16_t dif_u16(uint16_t a, uint16_t b)
{
    return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

// Avanza 'act' hacia 'obj' incrementando/decrementando como maximo 'paso'
static uint16_t acercar(uint16_t act, uint16_t obj, uint16_t paso)
{
    if (act < obj) return (obj - act > paso) ? (uint16_t)(act + paso) : obj;
    if (act > obj) return (act - obj > paso) ? (uint16_t)(act - paso) : obj;
    return act;
}

// Define las posiciones objetivo para ambos servos de la pinza
static void pinza_ir_a(uint16_t o1, uint16_t o2)
{
    ocr1Objetivo = o1;
    ocr2Objetivo = o2;
}

// Devuelve 1 si ambos servos ya alcanzaron exactamente las posiciones dadas
static uint8_t pinza_en(uint16_t o1, uint16_t o2)
{
    return (ocr1Actual == o1 && ocr2Actual == o2);
}

// Interpola el movimiento de ambos servos para que inicien y terminen sincronizados
static void actualizar_servo(void)
{
    uint16_t d1 = dif_u16(ocr1Actual, ocr1Objetivo);           // Distancia restante servo 1
    uint16_t d2 = dif_u16(ocr2Actual, ocr2Objetivo);           // Distancia restante servo 2
    uint16_t p1 = d1 > SERVO_PASO ? SERVO_PASO : d1;           // Paso servo 1 limitado a SERVO_PASO
    uint16_t p2;

    if (d1 == 0) p2 = SERVO_PASO;                              // Si servo 1 ya llego, mueve servo 2 normal
    else         p2 = (uint16_t)(((uint32_t)d2 * p1 + d1 - 1) / d1); // Paso proporcional para llegar juntos

    ocr1Actual = acercar(ocr1Actual, ocr1Objetivo, p1);
    ocr2Actual = acercar(ocr2Actual, ocr2Objetivo, p2);
    OCR1A = ocr1Actual;                                        // Actualiza pulso PWM servo 1
    OCR1B = ocr2Actual;                                        // Actualiza pulso PWM servo 2
}

/* ======================= PROTOCOLO ======================= */
// Parsea un entero con signo desde texto; devuelve puntero al siguiente caracter o NULL si falla
static const char *leer_entero(const char *p, int16_t *out)
{
    uint8_t neg = 0, hay = 0;
    int16_t v = 0;
    if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;
    while (*p >= '0' && *p <= '9') {
        if (v < 1000) v = v * 10 + (*p - '0');                 // Evita overflow en int16_t
        p++;
        hay = 1;
    }
    if (!hay) return 0;
    *out = neg ? -v : v;
    return p;
}

// Satura un valor dentro del rango [-100, 100]
static int16_t limitar100(int16_t x)
{
    if (x > 100) return 100;
    if (x < -100) return -100;
    return x;
}

// Actualiza consignas de movimiento si no hay paro de emergencia activo
static void set_mando(int16_t v, int16_t w)
{
    if (emergencia) return;
    cmd_v = limitar100(v);
    cmd_w = limitar100(w);
}

// Interpreta una trama completa terminada en '\n'
static void procesar_linea(const char *s)
{
    char c = s[0];
    int16_t a, b;
    const char *p;

    if (c >= 'a' && c <= 'z') c = (char)(c - 32);              // Convierte comando a mayuscula

    if (c == 'J' && s[1] == ',') {                             // Formato nuevo: "J,v,w"
        p = leer_entero(s + 2, &a);
        if (!p || *p != ',') return;
        p = leer_entero(p + 1, &b);
        if (!p) return;
        set_mando(a, b);
    }
    else {
        switch (c) {
            case 'O': if (!emergencia) pinza_ir_a(SERVO_MIN_OCR, SERVO2_ABIERTO_OCR);     break; // Abrir
            case 'C': if (!emergencia) pinza_ir_a(SERVO_MAX_OCR, SERVO2_CERRADO_OCR);     break; // Cerrar
            case 'G': if (!emergencia) pinza_ir_a(SERVO_GUARDAR_OCR, SERVO2_GUARDAR_OCR); break; // Guardar
            case 'S': parada_inmediata(); break;                                                 // Stop
            case 'E':                                                                            // Emergencia
                emergencia = 1;
                parada_inmediata();
                pinza_ir_a(ocr1Actual, ocr2Actual);            // Congela servos en su posicion actual
                break;
            case 'R': emergencia = 0; parada_inmediata(); break;                                 // Rearmar
            default:  return;                                  // Ignora comandos invalidos sin renovar timeout
        }
    }
    ultimo_cmd = millis();                                     // Reinicia contador de perdida de enlace
}

// Acumula caracteres entrantes hasta detectar fin de linea ('\n' o '\r')
static void recibir_byte(char c)
{
    if (c == '\n' || c == '\r') {
        if (linea_len > 0 && !linea_desbordada) {
            linea[linea_len] = '\0';                           // Cierra string C
            procesar_linea(linea);
        }
        linea_len = 0;                                         // Resetea buffer para la proxima trama
        linea_desbordada = 0;
    } else if (linea_len < LINEA_TAM - 1) {
        linea[linea_len++] = c;
    } else {
        linea_desbordada = 1;                                  // Marca error si la trama es demasiado larga
    }
}

// Envia estado de la pinza y sensor IR hacia la app ("T,garra,obstaculo\n")
static void enviar_telemetria(void)
{
    char garra = '0';                                          // 0 = abierta o moviendose
    if (pinza_en(SERVO_MAX_OCR, SERVO2_CERRADO_OCR))         garra = '1'; // 1 = cerrada
    else if (pinza_en(SERVO_GUARDAR_OCR, SERVO2_GUARDAR_OCR)) garra = '2'; // 2 = guardada

    tx_char('T'); tx_char(',');
    tx_char(garra); tx_char(',');
    tx_char(ir_obstaculo() ? '1' : '0');
    tx_char('\n');
}

/* ======================= TAREA PERIODICA (10 ms) ======================= */
static void tarea_10ms(uint32_t ahora)
{
    // Failsafe: si se pierde conexion Bluetooth por mas de 500 ms, frena
    if ((uint32_t)(ahora - ultimo_cmd) > TIMEOUT_ENLACE_MS) {
        cmd_v = 0;
        cmd_w = 0;
    }
    actualizar_traccion();
    actualizar_servo();
}

/* ======================= MAIN ======================= */
int main(void)
{
    USART_Init();                                              // Configura comunicacion serie
    Timer0_Motors_Init();                                      // Configura PWM de motores
    Timer1_Servo_Init();                                       // Configura PWM de servos (50 Hz)
    Timer2_Tick_Init();                                        // Configura base de tiempo de 1 ms
    Sensor_IR_Init();                                          // Configura pin del sensor IR
    parada_inmediata();                                        // Asegura motores frenados al encender
    sei();                                                     // Habilita interrupciones globales

    uint32_t t_tick = millis();                                // Temporizador para bucle de control (10 ms)
    uint32_t t_tel  = t_tick;                                  // Temporizador para telemetria (500 ms)
    ultimo_cmd = t_tick;

    for (;;) {
        uint8_t c;
        while (rx_leer(&c)) recibir_byte((char)c);             // Procesa todos los bytes pendientes del Bluetooth

        uint32_t ahora = millis();
        if ((uint32_t)(ahora - t_tick) >= 10) {                // Ejecuta control de motores y servos cada 10 ms
            t_tick += 10;
            tarea_10ms(ahora);
        }
        if ((uint32_t)(ahora - t_tel) >= 500) {                // Envia telemetria a la app cada 500 ms
            t_tel += 500;
            enviar_telemetria();
        }
    }
    return 0;
}
