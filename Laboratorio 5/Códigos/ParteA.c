#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

// --- DEFINICIÓN DE PINES ---
#define LED_PIN       PB5   // Pin 13 Arduino (LED de estado)
#define BUZZER_PIN    PB0   // Pin 8 Arduino (Buzzer)
#define BUTTON_PIN    PD2   // Pin 2 Arduino (INT0 - Pulsador)

// Mapeo del DHT11 en Pin 12 (PB4)
#define DHT_PORT      PORTB
#define DHT_DDR       DDRB
#define DHT_PIN       PINB
#define DHT_BIT       PB4   // Pin 12 Arduino

// Configuración LCD I2C (PCF8574 en A4/A5)
#define I2C_ADDR      0x27  // Dirección I2C habitual
#define LCD_BACKLIGHT 0x08  // Bit P3: Luz de fondo
#define LCD_ENABLE    0x04  // Bit P2: Enable
#define LCD_RS        0x01  // Bit P0: Register Select

// --- FSM ---
typedef enum {
	ESTADO_ACTIVO = 0,
	ESTADO_STANDBY,
	ESTADO_APAGADO
} EstadoSistema;

typedef void (*FuncionEstado)(void);

// --- VARIABLES GLOBALES VOLÁTILES ---
volatile uint32_t milisegundos_totales = 0;
volatile uint32_t ultimo_evento_usuario = 0;

volatile uint16_t cont_led_ms = 0;
volatile uint16_t cont_lcd_ms = 0;
volatile uint16_t cont_temp_ms = 0;

volatile bool flag_tarea_led = false;
volatile bool flag_tarea_lcd = false;
volatile bool flag_tarea_temp = false;
volatile bool flag_evento_boton = false;

volatile uint16_t valor_adc = 0;
volatile uint8_t dht_temp = 0;
volatile uint8_t dht_hum = 0;

EstadoSistema id_estado_actual = ESTADO_ACTIVO;

// --- GENERADOR DE TONO PARA BUZZER ---

void Buzzer_Beep(void) {
	for (uint16_t i = 0; i < 100; i++) {
		PORTB |= (1 << BUZZER_PIN);
		_delay_us(500);
		PORTB &= ~(1 << BUZZER_PIN);
		_delay_us(500);
	}
}

// --- HARDWARE TWI (I2C POR REGISTROS) ---

void TWI_Init(void) {
	TWSR = 0x00; // Prescaler = 1
	TWBR = 72;   // Frecuencia SCL = 100 kHz @ 16 MHz
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

// --- DRIVER LCD I2C ---

void lcd_send_nibble(uint8_t nibble, uint8_t es_dato) {
	uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT | (es_dato ? LCD_RS : 0);
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

	lcd_send_byte(0x28, 0); // 2 líneas, 5x8
	lcd_send_byte(0x0C, 0); // Display ON, Cursor OFF
	lcd_send_byte(0x01, 0); // Limpiar pantalla
	_delay_ms(2);
	lcd_send_byte(0x06, 0); // Auto-incremento
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

// --- RUTINA DHT11 INTEGRADA DEL OTRO LABORATORIO ---

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

	*hum = data[0];
	*temp = data[2];
	return 0;
}

// --- HARDWARE INIT ---

void Timer0_Init(void) {
	TCCR0A = (1 << WGM01);              // Modo CTC
	TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler 64
	OCR0A = 249;                        // Base de tiempo 1 ms
	TIMSK0 |= (1 << OCIE0A);
}

void Timer1_Init(void) {
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12); // Prescaler 256
	OCR1A = 6249;                        // Interrupción cada 100 ms
	TIMSK1 |= (1 << OCIE1A);
}

void ADC_Init(void) {
	ADMUX = (1 << REFS0);               // Referencia AVcc (5V), Canal ADC0 (A0)
	ADCSRA = (1 << ADEN)  | (1 << ADIE) |
	(1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Prescaler 128
}

void INT0_Init(void) {
	EICRA |= (1 << ISC01);              // Flanco de bajada
	EIMSK |= (1 << INT0);
}

// --- ISRs ---

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
	if (id_estado_actual == ESTADO_ACTIVO) {
		ADCSRA |= (1 << ADSC); // Conversión ADC cada 100 ms
	}
}

ISR(ADC_vect) {
	valor_adc = ADC;
}

ISR(INT0_vect) {
	flag_evento_boton = true;
	ultimo_evento_usuario = milisegundos_totales;
}

// --- TAREAS Y FSM ---

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

void estado_activo(void) {
	if (flag_evento_boton) {
		flag_evento_boton = false;
		Buzzer_Beep();
	}

	if (flag_tarea_led)  { flag_tarea_led = false;  Tarea_LED(); }
	if (flag_tarea_temp) { flag_tarea_temp = false; Tarea_Temperatura(); }
	if (flag_tarea_lcd)  { flag_tarea_lcd = false;  Tarea_LCD_Update(); }

	if (milisegundos_totales - ultimo_evento_usuario >= 10000) {
		id_estado_actual = ESTADO_STANDBY;
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("Esperando...");
	}
}

void estado_standby(void) {
	if (flag_evento_boton) {
		flag_evento_boton = false;
		Buzzer_Beep();

		id_estado_actual = ESTADO_ACTIVO;
		LCD_Clear();
		return;
	}

	if (milisegundos_totales - ultimo_evento_usuario >= 30000) {
		id_estado_actual = ESTADO_APAGADO;
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("APAGADO");
	}
}

void estado_apagado(void) {
	PORTB &= ~(1 << LED_PIN);

	if (flag_evento_boton) {
		flag_evento_boton = false;
		Buzzer_Beep();

		id_estado_actual = ESTADO_ACTIVO;
		LCD_Clear();
	}
}

FuncionEstado fsm_estados[] = { estado_activo, estado_standby, estado_apagado };

int main(void) {
	DDRB |= (1 << LED_PIN) | (1 << BUZZER_PIN);
	DDRD &= ~(1 << BUTTON_PIN);
	PORTD |= (1 << BUTTON_PIN); // Pull-Up en PD2

	LCD_Init();
	Timer0_Init();
	Timer1_Init();
	ADC_Init();
	INT0_Init();

	sei();

	Tarea_LCD_Update();

	while (1) {
		fsm_estados[id_estado_actual]();
	}
}
