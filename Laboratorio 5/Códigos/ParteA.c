// Define la frecuencia de reloj del microcontrolador (16 MHz) para calcular retardos (_delay_ms / _delay_us)
#define F_CPU 16000000UL

// Librería principal para el acceso a registros de E/S del ATmega328P (PORTx, DDRx, PINx)
#include <avr/io.h>

// Librería para la gestión de interrupciones del sistema (sei, cli, ISR)
#include <avr/interrupt.h>

// Librería con funciones de retardo por software basados en F_CPU
#include <util/delay.h>

// Librería estandar de C para usar el tipo de dato booleano (bool, true, false)
#include <stdbool.h>

// Librería estándar de C para formateo de cadenas de texto (snprintf)
#include <stdio.h>

// --- DEFINICIÓN DE PINES DEL SISTEMA ---
// Pin 13 de Arduino (Puerto B, Bit 5): LED integrado indicador de estado
#define LED_PIN        PB5

// Pin 8 de Arduino (Puerto B, Bit 0): Salida digital para el buzzer pasivo
#define BUZZER_PIN     PB0

// Pin 2 de Arduino (Puerto D, Bit 2): Entrada de interrupción externa INT0 para el pulsador
#define BUTTON_PIN     PD2

// --- MAPEO Y REGISTROS DEL SENSOR DHT11 ---
// Puerto de salida de datos del sensor DHT11 (Puerto B)
#define DHT_PORT       PORTB

// Registro de dirección de datos para el DHT11 (0=Entrada, 1=Salida)
#define DHT_DDR        DDRB

// Registro de lectura del pin del DHT11
#define DHT_PIN        PINB

// Pin 12 de Arduino (Puerto B, Bit 4): Pin de datos del sensor DHT11
#define DHT_BIT        PB4

// --- CONFIGURACIÓN DEL DISPLAY LCD I2C (EXPANSOR PCF8574) ---
// Dirección del bus I2C para el módulo PCF8574 (0x27 habitual o 0x3F)
#define I2C_ADDR       0x27

// Máscara para encender la luz de fondo del LCD (Bit P3 del PCF8574)
#define LCD_BACKLIGHT  0x08

// Máscara del bit Enable (E) del LCD para validar datos/comandos (Bit P2 del PCF8574)
#define LCD_ENABLE     0x04

// Máscara del bit Register Select (RS): 0 = Comando, 1 = Dato (Bit P0 del PCF8574)
#define LCD_RS         0x01

// --- MÁQUINA DE ESTADOS FINITA (FSM) ---
// Definición del tipo enumerado para los posibles estados del sistema
typedef enum {
	ESTADO_ACTIVO = 0,  // Estado normal de funcionamiento total (LED, ADC, DHT11 y LCD activos)
	ESTADO_STANDBY,     // Estado de espera con bajo consumo de pantalla ("Esperando...")
	ESTADO_APAGADO      // Estado inactivo con periféricos desactivados
} EstadoSistema;

// Definición de tipo para punteros a funciones de estado (sin parámetros ni retorno)
typedef void (*FuncionEstado)(void);

// --- VARIABLES GLOBALES VOLÁTILES (COMPARTIDAS CON ISR) ---
// Contador global de tiempo del sistema en milisegundos desde el inicio
volatile uint32_t milisegundos_totales = 0;

// Registra la marca de tiempo (en ms) en la que ocurrió la última acción del usuario
volatile uint32_t ultimo_evento_usuario = 0;

// Contador incremental para la temporización de la tarea del LED (en ms)
volatile uint16_t cont_led_ms = 0;

// Contador incremental para la temporización del refresco del LCD (en ms)
volatile uint16_t cont_lcd_ms = 0;

// Contador incremental para la temporización de lectura del DHT11 (en ms)
volatile uint16_t cont_temp_ms = 0;

// Flag que indica al bucle principal que debe ejecutar el parpadeo del LED
volatile bool flag_tarea_led = false;

// Flag que indica al bucle principal que debe actualizar el contenido del LCD
volatile bool flag_tarea_lcd = false;

// Flag que indica al bucle principal que debe leer el sensor DHT11
volatile bool flag_tarea_temp = false;

// Flag de evento que se activa mediante interrupción al presionar el pulsador
volatile bool flag_evento_boton = false;

// Almacena la última lectura leída del convertidor analógico a digital (ADC 0-1023)
volatile uint16_t valor_adc = 0;

// Almacena el valor de temperatura leída en grados Celsius por el DHT11
volatile uint8_t dht_temp = 0;

// Almacena el valor de humedad relativa expresada en porcentaje leída por el DHT11
volatile uint8_t dht_hum = 0;

// Variable global que almacena el estado actual de la FSM (Inicia en ACTIVO)
EstadoSistema id_estado_actual = ESTADO_ACTIVO;

// --- GENERADOR DE TONO PARA BUZZER ---
// Emite un sonido tipo bip activando/desactivando el pin del buzzer con retardos
void Buzzer_Beep(void) {
	for (uint16_t i = 0; i < 100; i++) {       // Repite 100 ciclos para dar la duración al tono
		PORTB |= (1 << BUZZER_PIN);            // Pone en nivel alto (5V) el pin del buzzer
		_delay_us(500);                        // Espera 500 microsegundos (semiciclo alto)
		PORTB &= ~(1 << BUZZER_PIN);           // Pone en nivel bajo (0V) el pin del buzzer
		_delay_us(500);                        // Espera 500 microsegundos (semiciclo bajo) -> Frecuencia = 1 kHz
	}
}

// --- DRIVER HARDWARE TWI (I2C) POR REGISTROS ---
// Inicializa la interfaz hardware I2C del microcontrolador
void TWI_Init(void) {
	TWSR = 0x00; // Prescaler del TWI en 1 (bits TWPS1=0, TWPS0=0)
	TWBR = 72;   // Define la frecuencia de SCL a 100 kHz con F_CPU = 16 MHz
	TWCR = (1 << TWEN); // Habilita físicamente el periférico TWI/I2C
}

// Genera la condición de INICIO (START) en el bus I2C
void TWI_Start(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // Limpia flag, envía START y habilita TWI
	while (!(TWCR & (1 << TWINT)));                   // Espera hasta que termine la transmisión del START
}

// Genera la condición de PARADA (STOP) en el bus I2C
void TWI_Stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // Limpia flag, envía STOP y habilita TWI
}

// Envía un byte de datos por el bus I2C
void TWI_Write(uint8_t data) {
	TWDR = data;                                      // Carga el byte a transmitir en el registro de datos TWI
	TWCR = (1 << TWINT) | (1 << TWEN);                // Inicia la transmisión del byte
	while (!(TWCR & (1 << TWINT)));                   // Espera a que finalice la transmisión
}

// Escribe un byte completo de datos en el expansor I2C PCF8574
void pcf8574_write(uint8_t data) {
	TWI_Start();                                      // Genera condición de Inicio
	TWI_Write((I2C_ADDR << 1) | 0);                   // Envía dirección de 7 bits del PCF8574 + Bit 0 (Escritura)
	TWI_Write(data);                                  // Envía los 8 bits hacia las salidas P0-P7 del PCF8574
	TWI_Stop();                                       // Genera condición de Parada
}

// --- DRIVER PANTALLA LCD MODO 4 BITS VÍA I2C ---
// Envía 4 bits (un nibble) al LCD agregando las señales de control (RS, Enable y Backlight)
void lcd_send_nibble(uint8_t nibble, uint8_t es_dato) {
	// Combina los 4 bits altos de datos con el estado de retroiluminación y RS (Comando o Dato)
	uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT | (es_dato ? LCD_RS : 0);
	
	pcf8574_write(data | LCD_ENABLE); // Envía los datos activando el pin Enable (E = 1)
	_delay_us(1);                     // Mantiene el pulso de Enable activo durante más de 450 ns
	pcf8574_write(data & ~LCD_ENABLE);// Desactiva el pin Enable (E = 0) para flanco de bajada que valida el byte
	_delay_us(50);                    // Retardo de procesamiento para comandos estándar
}

// Envía un byte completo dividiéndolo en dos nibbles (4 bits altos y 4 bits bajos)
void lcd_send_byte(uint8_t byte, uint8_t es_dato) {
	lcd_send_nibble(byte & 0xF0, es_dato);        // Envía el nibble más significativo
	lcd_send_nibble((byte << 4) & 0xF0, es_dato); // Desplaza los bits bajos a la posición alta y los envía
}

// Configura el LCD en modo de interfaz de 4 bits mediante la secuencia oficial de reinicio
void LCD_Init(void) {
	TWI_Init();  // Inicializa el bus I2C
	_delay_ms(50); // Espera que el voltaje del LCD se estabilice tras el encendido (>15 ms)

	// Secuencia de reinicio por hardware especificada en el datasheet del controlador HD44780
	lcd_send_nibble(0x30, 0); _delay_ms(5);     // Comando de inicialización 1
	lcd_send_nibble(0x30, 0); _delay_ms(5);     // Comando de inicialización 2
	lcd_send_nibble(0x30, 0); _delay_us(150);   // Comando de inicialización 3
	lcd_send_nibble(0x20, 0); _delay_ms(2);     // Establece la interfaz en modo 4 bits

	lcd_send_byte(0x28, 0); // Configuración: 2 líneas, fuente de matriz 5x8
	lcd_send_byte(0x0C, 0); // Enciende la pantalla (Display ON), oculta el cursor (Cursor OFF)
	lcd_send_byte(0x01, 0); // Limpia toda la pantalla (Clear Display)
	_delay_ms(2);           // El comando de limpiar requiere un retardo mayor de 1.52 ms
	lcd_send_byte(0x06, 0); // Modo de entrada: Auto-incremento de dirección del cursor hacia la derecha
}

// Mueve la posición de escritura del cursor del LCD a las coordenadas indicadas
void LCD_SetCursor(uint8_t fila, uint8_t columna) {
	// Fila 0 inicia en dirección 0x80; Fila 1 inicia en dirección 0xC0 de la memoria DDRAM
	uint8_t addr = (fila == 0) ? (0x80 + columna) : (0xC0 + columna);
	lcd_send_byte(addr, 0); // Envía comando para mover la posición de memoria DDRAM
}

// Muestra una cadena de caracteres terminada en nulo en el LCD
void LCD_Print(const char* str) {
	while (*str) lcd_send_byte(*str++, 1); // Recorre el string enviando cada carácter como dato (RS=1)
}

// Limpia el contenido proyectado en el LCD
void LCD_Clear(void) {
	lcd_send_byte(0x01, 0); // Envió del comando de limpieza
	_delay_ms(2);           // Tiempo de espera necesario para ejecutar la limpieza
}

// --- PROTOCOLO Y LECTURA DEL SENSOR DE TEMPERATURA Y HUMEDAD DHT11 ---
// Espera a que el pin del DHT11 alcance un estado lógico (0 o 1) con tiempo límite (timeout)
uint8_t dht_wait_level(uint8_t nivel, uint16_t timeout_us) {
	// Compara el estado actual del pin DHT_BIT con el valor del nivel esperado
	while ((((DHT_PIN & (1 << DHT_BIT)) != 0) ? 1 : 0) != nivel) {
		_delay_us(1);            // Retardo de 1 microsegundo
		if (timeout_us == 0) return 0; // Si se agotó el tiempo límite, retorna 0 (Error de timeout)
		timeout_us--;            // Decrementa el contador de tiempo límite
	}
	return 1;                    // Cambió de nivel a tiempo, retorna 1 (Éxito)
}

// Realiza el protocolo de lectura del DHT11 y extrae los valores de Humedad y Temperatura
uint8_t dht11_read(uint8_t *hum, uint8_t *temp) {
	uint8_t data[5] = {0, 0, 0, 0, 0}; // Arreglo para almacenar los 5 bytes de respuesta (4 de datos + 1 Checksum)
	uint8_t i, j;
	uint16_t ancho;

	cli(); // Deshabilita interrupciones globales para garantizar la precisión de tiempos del bus de 1 hilo

	// --- SEÑAL DE INICIO DEL MICROCONTROLADOR AL DHT11 ---
	DHT_DDR |= (1 << DHT_BIT);   // Configura el pin como salida
	DHT_PORT &= ~(1 << DHT_BIT);  // Tira la línea a nivel bajo (0V)
	_delay_ms(20);               // Mantiene en bajo al menos 18 ms para iniciar el sensor

	DHT_PORT |= (1 << DHT_BIT);   // Sube la línea a nivel alto (5V)
	_delay_us(30);               // Espera 20-40 microsegundos

	DHT_DDR &= ~(1 << DHT_BIT);  // Configura el pin como entrada para recibir respuesta
	DHT_PORT |= (1 << DHT_BIT);  // Activa la resistencia interna de Pull-up

	// --- RESPUESTA DEL DHT11 ---
	if (!dht_wait_level(0, 200)) { sei(); return 1; } // Espera la respuesta en bajo (80us)
	if (!dht_wait_level(1, 200)) { sei(); return 2; } // Espera la respuesta en alto (80us)
	if (!dht_wait_level(0, 200)) { sei(); return 3; } // Espera el fin del pulso de preparación

	// --- LECTURA DE LOS 40 BITS DE DATOS DE LA TRAMA ---
	for (j = 0; j < 5; j++) {         // Iteración para recibir los 5 bytes del protocolo
		for (i = 0; i < 8; i++) {     // Iteración para leer los 8 bits de cada byte
			if (!dht_wait_level(1, 100)) { sei(); return 4; } // Espera el inicio del bit en alto
			ancho = 0;
			// Mide la duración del pulso en nivel alto para distinguir un 0 de un 1
			while (DHT_PIN & (1 << DHT_BIT)) {
				_delay_us(1);
				ancho++;
				if (ancho >= 100) break; // Límite de seguridad
			}
			// Si el pulso en alto dura más de 40 us, corresponde a un bit '1' (26-28us = '0', 70us = '1')
			if (ancho > 40) {
				data[j] |= (1 << (7 - i)); // Desplaza e inserta un bit '1' en la posición correspondiente
			}
		}
	}

	sei(); // Rehabilita las interrupciones globales una vez finalizada la sincronía crítica

	// --- VERIFICACIÓN DE CHECKSUM ---
	// Comprueba si la suma de los primeros 4 bytes es igual al byte 5 (Checksum)
	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
		return 6; // Retorna error si falla el Checksum
	}

	*hum = data[0];  // Asigna la parte entera de la humedad
	*temp = data[2]; // Asigna la parte entera de la temperatura
	return 0;        // Retorna 0 indicando lectura exitosa
}

// --- CONFIGURACIÓN DE PERIFÉRICOS HARDWARE ---
// Configura el Timer0 en modo CTC para generar una base de tiempo de 1 ms
void Timer0_Init(void) {
	TCCR0A = (1 << WGM01);              // Modo CTC (Clear Timer on Compare Match)
	TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler de 64
	OCR0A = 249;                        // (16 MHz / (64 * (249 + 1))) = 1000 Hz -> Interrupción cada 1 ms
	TIMSK0 |= (1 << OCIE0A);            // Habilita la interrupción por comparación del canal A de Timer0
}

// Configura el Timer1 en modo CTC para generar una base de tiempo de 100 ms
void Timer1_Init(void) {
	TCCR1A = 0;                          // Configuración normal de salidas de comparación deshabilitadas
	TCCR1B = (1 << WGM12) | (1 << CS12); // Modo CTC y Prescaler de 256
	OCR1A = 6249;                        // (16 MHz / (256 * (6249 + 1))) = 10 Hz -> Interrupción cada 100 ms
	TIMSK1 |= (1 << OCIE1A);             // Habilita la interrupción por comparación del canal A de Timer1
}

// Configura el periférico analógico a digital (ADC)
void ADC_Init(void) {
	ADMUX = (1 << REFS0);               // Referencia de voltaje AVcc (5V), selecciona el canal ADC0 (Pin A0)
	ADCSRA = (1 << ADEN)  | (1 << ADIE) |
	(1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enciende ADC, activa su ISR y prescaler de 128 (125 kHz)
}

// Configura la interrupción externa INT0 en el pin PD2
void INT0_Init(void) {
	EICRA |= (1 << ISC01);              // Configura la detección por flanco de bajada (Falling edge)
	EIMSK |= (1 << INT0);               // Habilita la interrupción externa INT0
}

// --- RUTINAS DE SERVICIO DE INTERRUPCIÓN (ISRs) ---
// ISR ejecutada automáticamente cada 1 ms por Timer0
ISR(TIMER0_COMPA_vect) {
	milisegundos_totales++; // Incrementa el contador principal de tiempo de ejecucion

	// Solo administra contadores de tareas periódicas cuando el sistema está en estado ACTIVO
	if (id_estado_actual == ESTADO_ACTIVO) {
		cont_led_ms++;
		if (cont_led_ms >= 200) { cont_led_ms = 0; flag_tarea_led = true; } // Tarea LED cada 200 ms

		cont_lcd_ms++;
		if (cont_lcd_ms >= 500) { cont_lcd_ms = 0; flag_tarea_lcd = true; } // Tarea LCD cada 500 ms

		cont_temp_ms++;
		if (cont_temp_ms >= 2000) { cont_temp_ms = 0; flag_tarea_temp = true; } // Tarea DHT11 cada 2000 ms (2s)
	}
}

// ISR ejecutada automáticamente cada 100 ms por Timer1
ISR(TIMER1_COMPA_vect) {
	if (id_estado_actual == ESTADO_ACTIVO) {
		ADCSRA |= (1 << ADSC); // Inicia una nueva conversión analógica a digital (ADC) cada 100 ms
	}
}

// ISR ejecutada cuando la conversión analógica a digital del ADC finaliza
ISR(ADC_vect) {
	valor_adc = ADC; // Lee y guarda el resultado completo del registro de 10 bits del ADC
}

// ISR ejecutada cuando se presiona el botón físico conectado a INT0 (Pin 2)
ISR(INT0_vect) {
	flag_evento_boton = true;                   // Registra que el usuario presionó el botón
	ultimo_evento_usuario = milisegundos_totales; // Registra la marca de tiempo de la interacción
}

// --- TAREAS SECUNDARIAS ---
// Tarea de conmutación de estado del LED de prueba
void Tarea_LED(void) {
	PORTB ^= (1 << LED_PIN); // Invierte el estado lógico del pin del LED (On/Off)
}

// Tarea de lectura de temperatura y humedad
void Tarea_Temperatura(void) {
	uint8_t t = 0, h = 0;
	if (dht11_read(&h, &t) == 0) { // Lee el sensor; si la lectura es válida actualiza globales
		dht_temp = t;
		dht_hum  = h;
	}
}

// Tarea de formateo e impresión de datos sobre la pantalla LCD
void Tarea_LCD_Update(void) {
	char buffer[17]; // Buffer auxiliar de 17 posiciones para generar la cadena impresiva (16 caracteres + NULL)

	LCD_SetCursor(0, 0);             // Posiciona en la Fila 0, Columna 0
	LCD_Print("Estado: ACTIVO  ");   // Escribe el estado actual

	LCD_SetCursor(1, 0);             // Posiciona en la Fila 1, Columna 0
	// Formatea la cadena de texto con la temperatura y el valor entero del ADC a 4 dígitos
	snprintf(buffer, sizeof(buffer), "Temp:%02dC A0:%04d", dht_temp, valor_adc);
	LCD_Print(buffer);               // Imprime la cadena formateada en la segunda línea
}

// --- MAQUINAS DE ESTADO (Manejadores de cada Estado) ---
// Manejador del estado ACTIVO
void estado_activo(void) {
	if (flag_evento_boton) {
		flag_evento_boton = false; // Atiende el evento borrando la bandera
		Buzzer_Beep();             // Feedback sonoro
	}

	// Ejecuta las tareas asociadas a sus respectivas banderas
	if (flag_tarea_led)  { flag_tarea_led = false;  Tarea_LED(); }
	if (flag_tarea_temp) { flag_tarea_temp = false; Tarea_Temperatura(); }
	if (flag_tarea_lcd)  { flag_tarea_lcd = false;  Tarea_LCD_Update(); }

	// Si han transcurrido 10 segundos (10000 ms) sin presionar el botón, pasa a STANDBY
	if (milisegundos_totales - ultimo_evento_usuario >= 10000) {
		id_estado_actual = ESTADO_STANDBY; // Cambia el puntero de estado
		LCD_Clear();                       // Limpia pantalla
		LCD_SetCursor(0, 0);
		LCD_Print("Esperando...");         // Imprime mensaje del estado standby
	}
}

// Manejador del estado STANDBY
void estado_standby(void) {
	if (flag_evento_boton) {
		flag_evento_boton = false; // Atiende el evento borrando la bandera
		Buzzer_Beep();             // Emite tono de confirmación

		id_estado_actual = ESTADO_ACTIVO; // Vuelve al estado ACTIVO si el usuario presiona el botón
		LCD_Clear();
		return;
	}

	// Si se alcanzan 30 segundos (30000 ms) de inactividad global, pasa al estado APAGADO
	if (milisegundos_totales - ultimo_evento_usuario >= 30000) {
		id_estado_actual = ESTADO_APAGADO; // Cambia a estado APAGADO
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("APAGADO");              // Muestra el mensaje de apagado
	}
}

// Manejador del estado APAGADO
void estado_apagado(void) {
	PORTB &= ~(1 << LED_PIN); // Apaga obligatoriamente el LED indicador

	if (flag_evento_boton) {
		flag_evento_boton = false; // Atiende el evento de botón
		Buzzer_Beep();             // Emite el bip de encendido

		id_estado_actual = ESTADO_ACTIVO; // Reactiva el sistema regresando al estado ACTIVO
		LCD_Clear();                      // Limpia la pantalla
	}
}

// Arreglo de punteros a funciones que mapea el enum EstadoSistema con su función correspondientes
FuncionEstado fsm_estados[] = { estado_activo, estado_standby, estado_apagado };

// --- FUNCIÓN PRINCIPAL MAIN ---
int main(void) {
	// Configuración de direcciones de pines de Entrada/Salida
	DDRB |= (1 << LED_PIN) | (1 << BUZZER_PIN); // Define LED (PB5) y Buzzer (PB0) como Salidas
	DDRD &= ~(1 << BUTTON_PIN);                 // Define Botón (PD2) como Entrada
	PORTD |= (1 << BUTTON_PIN);                // Activa la resistencia interna de Pull-Up en el botón (PD2)

	// Inicialización de submódulos de hardware
	LCD_Init();    // Inicializa la pantalla LCD
	Timer0_Init(); // Inicializa Timer0 (Temporizador base de 1 ms)
	Timer1_Init(); // Inicializa Timer1 (Disparador del ADC cada 100 ms)
	ADC_Init();    // Inicializa el convertidor analógico a digital
	INT0_Init();   // Inicializa la interrupción por hardware del pulsador

	sei(); // Habilita globalmente las interrupciones en el registro SREG (I-bit)

	Tarea_LCD_Update(); // Primera renderización de la interfaz en LCD

	// Bucle infinito del programa
	while (1) {
		// Llama a la función del estado actual según el índice `id_estado_actual`
		fsm_estados[id_estado_actual]();
	}
}
