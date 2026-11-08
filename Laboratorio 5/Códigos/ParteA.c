// Define la frecuencia de reloj del microcontrolador (16 MHz) para calcular retardos (_delay_ms / _delay_us)
#define F_CPU 16000000UL

// Librerías principales de AVR C
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

// --- DEFINICIÓN DE PINES DEL SISTEMA ---
#define LED_PIN              PB5   // Pin 13 Arduino (LED integrado)
#define BUZZER_PIN           PB0   // Pin 8 Arduino (Buzzer pasivo)
#define BUTTON_PIN           PD2   // Pin 2 Arduino (Entrada INT0 - Botón de Navegación)
#define EMERGENCY_BUTTON_PIN PD3   // Pin 3 Arduino (Entrada INT1 - Botón de Emergencia)

// --- MAPEO Y REGISTROS DEL SENSOR DHT11 ---
#define DHT_PORT        PORTB
#define DHT_DDR         DDRB
#define DHT_PIN         PINB
#define DHT_BIT         PB4   // Pin 12 Arduino

// --- CONFIGURACIÓN DEL DISPLAY LCD I2C (EXPANSOR PCF8574) ---
#define I2C_ADDR        0x27
#define LCD_ENABLE      0x04   // Máscara del bit Enable (P2)
#define LCD_RS          0x01   // Máscara del bit RS (P0)

// --- MÁQUINA DE ESTADOS FINITA (FSM) ---
typedef enum {
	ESTADO_ACTIVO = 0,  // Estado normal (LED, ADC, DHT11 y LCD activos)
	ESTADO_STANDBY,     // Estado de espera ("Esperando...")
	ESTADO_APAGADO,     // Estado inactivo (LCD retroiluminación apagada y ADC deshabilitado)
	ESTADO_EMERGENCIA   // Estado seguro ante solicitud de emergencia
} EstadoSistema;

typedef void (*FuncionEstado)(void);

// --- VARIABLES GLOBALES VOLÁTILES (COMPARTIDAS CON ISR) ---
volatile uint32_t milisegundos_totales = 0;
volatile uint32_t ultimo_evento_usuario = 0;

volatile uint16_t cont_led_ms = 0;
volatile uint16_t cont_lcd_ms = 0;
volatile uint16_t cont_temp_ms = 0;

volatile bool flag_tarea_led = false;
volatile bool flag_tarea_lcd = false;
volatile bool flag_tarea_temp = false;

volatile bool flag_evento_boton = false;
volatile bool flag_evento_emergencia = false;

volatile uint16_t valor_adc = 0;
volatile uint8_t dht_temp = 0;
volatile uint8_t dht_hum = 0;

volatile EstadoSistema id_estado_actual = ESTADO_ACTIVO;

// Máscara dinámica para la retroiluminación del LCD (0x08 = Encendida, 0x00 = Apagada)
volatile uint8_t lcd_backlight_mask = 0x08;

// --- GENERADOR DE TONOS PARA BUZZER ---
void Buzzer_Beep(void) {
	for (uint16_t i = 0; i < 100; i++) {
		PORTB |= (1 << BUZZER_PIN);
		_delay_us(500);
		PORTB &= ~(1 << BUZZER_PIN);
		_delay_us(500);
	}
}

void Buzzer_Alarm(void) {
	for (uint16_t i = 0; i < 300; i++) {
		PORTB |= (1 << BUZZER_PIN);
		_delay_us(250);
		PORTB &= ~(1 << BUZZER_PIN);
		_delay_us(250);
	}
}

// --- DRIVER HARDWARE TWI (I2C) POR REGISTROS ---
void TWI_Init(void) {
	TWSR = 0x00;
	TWBR = 72; // Configura SCL a 100 kHz con F_CPU = 16 MHz
	TWCR = (1 << TWEN);
}

void TWI_Start(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void TWI_Stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void TWI_Write(uint8_t data) {
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void pcf8574_write(uint8_t data) {
	TWI_Start();
	TWI_Write((I2C_ADDR << 1) | 0);
	TWI_Write(data);
	TWI_Stop();
}

// --- CONTROL DE RETROILUMINACIÓN LCD ---
void LCD_BacklightOn(void) {
	lcd_backlight_mask = 0x08;
	pcf8574_write(lcd_backlight_mask);
}

void LCD_BacklightOff(void) {
	lcd_backlight_mask = 0x00;
	pcf8574_write(lcd_backlight_mask);
}

// --- DRIVER PANTALLA LCD MODO 4 BITS VÍA I2C ---
void lcd_send_nibble(uint8_t nibble, uint8_t es_dato) {
	uint8_t data = (nibble & 0xF0) | lcd_backlight_mask | (es_dato ? LCD_RS : 0);
	
	pcf8574_write(data | LCD_ENABLE);
	_delay_us(1);
	pcf8574_write(data & ~LCD_ENABLE);
	_delay_us(50);
}

void lcd_send_byte(uint8_t byte, uint8_t es_dato) {
	lcd_send_nibble(byte & 0xF0, es_dato);
	lcd_send_nibble((byte << 4) & 0xF0, es_dato);
}

void LCD_Init(void) {
	TWI_Init();
	_delay_ms(50);

	lcd_send_nibble(0x30, 0); _delay_ms(5);
	lcd_send_nibble(0x30, 0); _delay_ms(5);
	lcd_send_nibble(0x30, 0); _delay_us(150);
	lcd_send_nibble(0x20, 0); _delay_ms(2);

	lcd_send_byte(0x28, 0); // 2 líneas, fuente 5x8
	lcd_send_byte(0x0C, 0); // Display ON, Cursor OFF
	lcd_send_byte(0x01, 0); // Clear Display
	_delay_ms(2);
	lcd_send_byte(0x06, 0); // Incremento automático del cursor
}

void LCD_SetCursor(uint8_t fila, uint8_t columna) {
	uint8_t addr = (fila == 0) ? (0x80 + columna) : (0xC0 + columna);
	lcd_send_byte(addr, 0);
}

void LCD_Print(const char* str) {
	while (*str) lcd_send_byte(*str++, 1);
}

void LCD_Clear(void) {
	lcd_send_byte(0x01, 0);
	_delay_ms(2);
}

// --- PROTOCOLO Y LECTURA DEL SENSOR DHT11 ---
uint8_t dht_wait_level(uint8_t nivel, uint16_t timeout_us) {
	while ((((DHT_PIN & (1 << DHT_BIT)) != 0) ? 1 : 0) != nivel) {
		_delay_us(1);
		if (timeout_us == 0) return 0;
		timeout_us--;
	}
	return 1;
}

uint8_t dht11_read(uint8_t *hum, uint8_t *temp) {
	uint8_t data[5] = {0, 0, 0, 0, 0};
	uint8_t i, j;
	uint16_t ancho;

	cli();

	DHT_DDR |= (1 << DHT_BIT);
	DHT_PORT &= ~(1 << DHT_BIT);
	_delay_ms(20);

	DHT_PORT |= (1 << DHT_BIT);
	_delay_us(30);

	DHT_DDR &= ~(1 << DHT_BIT);
	DHT_PORT |= (1 << DHT_BIT);

	if (!dht_wait_level(0, 200)) { sei(); return 1; }
	if (!dht_wait_level(1, 200)) { sei(); return 2; }
	if (!dht_wait_level(0, 200)) { sei(); return 3; }

	for (j = 0; j < 5; j++) {
		for (i = 0; i < 8; i++) {
			if (!dht_wait_level(1, 100)) { sei(); return 4; }
			ancho = 0;
			while (DHT_PIN & (1 << DHT_BIT)) {
				_delay_us(1);
				ancho++;
				if (ancho >= 100) break;
			}
			if (ancho > 40) {
				data[j] |= (1 << (7 - i));
			}
		}
	}

	sei();

	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
		return 6;
	}

	*temp = data[2];
	*hum  = data[0];
	return 0;
}

// --- CONFIGURACIÓN DE PERIFÉRICOS HARDWARE ---
void Timer0_Init(void) {
	TCCR0A = (1 << WGM01);               // Modo CTC
	TCCR0B = (1 << CS01) | (1 << CS00);  // Prescaler de 64
	OCR0A = 249;                         // Base de tiempo de 1 ms
	TIMSK0 |= (1 << OCIE0A);
}

void Timer1_Init(void) {
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12); // Modo CTC, Prescaler de 256
	OCR1A = 6249;                        // Interrupción cada 100 ms
	TIMSK1 |= (1 << OCIE1A);
}

void ADC_Init(void) {
	ADMUX = (1 << REFS0); // AVcc (5V), Canal ADC0
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

void External_Interrupts_Init(void) {
	// INT0 (Pin 2 - Botón Navegación) e INT1 (Pin 3 - Botón Emergencia) por flanco de bajada
	EICRA |= (1 << ISC01) | (1 << ISC11);
	EIMSK |= (1 << INT0) | (1 << INT1);
}

// --- RUTINAS DE SERVICIO DE INTERRUPCIÓN (ISRs) ---
ISR(TIMER0_COMPA_vect) {
	milisegundos_totales++;

	if (id_estado_actual == ESTADO_ACTIVO) {
		cont_led_ms++;
		if (cont_led_ms >= 200) { cont_led_ms = 0; flag_tarea_led = true; }

		cont_lcd_ms++;
		if (cont_lcd_ms >= 500) { cont_lcd_ms = 0; flag_tarea_lcd = true; }

		cont_temp_ms++;
		if (cont_temp_ms >= 2000) { cont_temp_ms = 0; flag_tarea_temp = true; }
	}
}

ISR(TIMER1_COMPA_vect) {
	if (id_estado_actual == ESTADO_ACTIVO && (ADCSRA & (1 << ADEN))) {
		ADCSRA |= (1 << ADSC);
	}
}

ISR(ADC_vect) {
	valor_adc = ADC;
}

ISR(INT0_vect) {
	flag_evento_boton = true;
	ultimo_evento_usuario = milisegundos_totales;
}

ISR(INT1_vect) {
	flag_evento_emergencia = true;
	ultimo_evento_usuario = milisegundos_totales;
}

// --- TAREAS SECUNDARIAS ---
void Tarea_LED(void) {
	PORTB ^= (1 << LED_PIN);
}

void Tarea_Temperatura(void) {
	uint8_t t = 0, h = 0;
	if (dht11_read(&h, &t) == 0) {
		dht_temp = t;
		dht_hum  = h;
	}
}

void Tarea_LCD_Update(void) {
	char buffer[17];

	LCD_SetCursor(0, 0);
	LCD_Print("Estado: ACTIVO  ");

	LCD_SetCursor(1, 0);
	snprintf(buffer, sizeof(buffer), "Temp:%02dC A0:%04d", dht_temp, valor_adc);
	LCD_Print(buffer);
}

// --- MÁQUINAS DE ESTADO (MANEJADORES DE CADA ESTADO) ---
void estado_activo(void) {
	// Transición universal a ESTADO_EMERGENCIA ante solicitud de emergencia
	if (flag_evento_emergencia) {
		flag_evento_emergencia = false;
		id_estado_actual = ESTADO_EMERGENCIA;
		Buzzer_Alarm();
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("! EMERGENCIA !  ");
		LCD_SetCursor(1, 0);
		LCD_Print("Estado Seguro   ");
		return;
	}

	if (flag_evento_boton) {
		flag_evento_boton = false;
		Buzzer_Beep();
	}

	if (flag_tarea_led)  { flag_tarea_led = false;  Tarea_LED(); }
	if (flag_tarea_temp) { flag_tarea_temp = false; Tarea_Temperatura(); }
	if (flag_tarea_lcd)  { flag_tarea_lcd = false;  Tarea_LCD_Update(); }

	// Transición ACTIVO -> STANDBY tras 10 segundos de inactividad
	if (milisegundos_totales - ultimo_evento_usuario >= 10000) {
		id_estado_actual = ESTADO_STANDBY;
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("Esperando...");
	}
}

void estado_standby(void) {
	// Transición universal a ESTADO_EMERGENCIA ante solicitud de emergencia
	if (flag_evento_emergencia) {
		flag_evento_emergencia = false;
		id_estado_actual = ESTADO_EMERGENCIA;
		Buzzer_Alarm();
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("! EMERGENCIA !  ");
		LCD_SetCursor(1, 0);
		LCD_Print("Estado Seguro   ");
		return;
	}

	if (flag_evento_boton) {
		flag_evento_boton = false;
		Buzzer_Beep();

		id_estado_actual = ESTADO_ACTIVO;
		LCD_Clear();
		return;
	}

	// Transición STANDBY -> APAGADO tras 30 segundos acumulados de inactividad
	if (milisegundos_totales - ultimo_evento_usuario >= 30000) {
		id_estado_actual = ESTADO_APAGADO;

		ADCSRA &= ~(1 << ADEN);     // Deshabilita ADC
		PORTB &= ~(1 << LED_PIN);   // Apaga LED

		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("APAGADO");
		_delay_ms(1000);
		LCD_Clear();
		LCD_BacklightOff();         // Apaga retroiluminación LCD
	}
}

void estado_apagado(void) {
	PORTB &= ~(1 << LED_PIN);

	// Transición universal a ESTADO_EMERGENCIA ante solicitud de emergencia
	if (flag_evento_emergencia) {
		flag_evento_emergencia = false;
		ADCSRA |= (1 << ADEN);      // Reactiva ADC
		LCD_BacklightOn();          // Reactiva retroiluminación LCD
		id_estado_actual = ESTADO_EMERGENCIA;
		Buzzer_Alarm();
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("! EMERGENCIA !  ");
		LCD_SetCursor(1, 0);
		LCD_Print("Estado Seguro   ");
		return;
	}

	if (flag_evento_boton) {
		flag_evento_boton = false;

		ADCSRA |= (1 << ADEN);      // Reactiva ADC
		LCD_BacklightOn();          // Reactiva retroiluminación LCD
		Buzzer_Beep();

		id_estado_actual = ESTADO_ACTIVO;
		LCD_Clear();
	}
}

void estado_emergencia(void) {
	// Encendido sostenido del LED como indicación visual de estado de falla/emergencia
	PORTB |= (1 << LED_PIN);

	// Recuperación/Restablecimiento: Pulsar el botón de emergencia o el pulsador normal restablece el sistema a ACTIVO
	if (flag_evento_emergencia || flag_evento_boton) {
		flag_evento_emergencia = false;
		flag_evento_boton = false;

		Buzzer_Beep();
		id_estado_actual = ESTADO_ACTIVO;
		LCD_Clear();
	}
}

// Arreglo de punteros a función (cumple restricción de no usar switch-case)
FuncionEstado fsm_estados[] = { estado_activo, estado_standby, estado_apagado, estado_emergencia };

// --- FUNCIÓN PRINCIPAL MAIN ---
int main(void) {
	// Configuración de direcciones E/S
	DDRB |= (1 << LED_PIN) | (1 << BUZZER_PIN);
	DDRD &= ~((1 << BUTTON_PIN) | (1 << EMERGENCY_BUTTON_PIN));
	PORTD |= (1 << BUTTON_PIN) | (1 << EMERGENCY_BUTTON_PIN); // Resistencias Pull-Up en PD2 y PD3

	// Inicialización de hardware
	LCD_Init();
	Timer0_Init();
	Timer1_Init();
	ADC_Init();
	External_Interrupts_Init(); // Configura INT0 e INT1

	sei(); // Habilita interrupciones globales

	Tarea_LCD_Update();

	while (1) {
		fsm_estados[id_estado_actual]();
	}
}
