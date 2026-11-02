#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

// --- DEFINICIÓN DE PINES ---
#define LED_PIN       PB5   // Pin 13 Arduino
#define BUZZER_PIN    PB0   // Pin 8 Arduino
#define BUTTON_PIN    PD2   // Pin 2 Arduino (INT0)
#define DHT11_PIN     PD3   // Pin 3 Arduino (Datos DHT11)

#define LCD_RS_PIN    PB1   // Pin 9 Arduino
#define LCD_E_PIN     PB2   // Pin 10 Arduino

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
volatile uint8_t dht_hum = 0; // Se lee en background para validación del DHT11

EstadoSistema id_estado_actual = ESTADO_ACTIVO;

// --- DRIVER LCD HD44780 ---

void lcd_send_nibble(uint8_t nibble) {
	PORTD = (PORTD & 0x0F) | (nibble & 0xF0);
	PORTB |= (1 << LCD_E_PIN);
	_delay_ms(2);
	PORTB &= ~(1 << LCD_E_PIN);
	_delay_ms(2);
}

void lcd_send_byte(uint8_t byte, uint8_t es_dato) {
	if (es_dato) PORTB |= (1 << LCD_RS_PIN);
	else         PORTB &= ~(1 << LCD_RS_PIN);

	lcd_send_nibble(byte & 0xF0);
	lcd_send_nibble((byte << 4) & 0xF0);
}

void LCD_Init(void) {
	DDRD |= 0xF0;
	DDRB |= (1 << LCD_RS_PIN) | (1 << LCD_E_PIN);
	PORTB &= ~((1 << LCD_RS_PIN) | (1 << LCD_E_PIN));
	_delay_ms(100);

	lcd_send_nibble(0x30); _delay_ms(10);
	lcd_send_nibble(0x30); _delay_ms(5);
	lcd_send_nibble(0x30); _delay_ms(5);
	lcd_send_nibble(0x20); _delay_ms(5);

	lcd_send_byte(0x28, 0);
	lcd_send_byte(0x0C, 0);
	lcd_send_byte(0x01, 0);
	_delay_ms(10);
	lcd_send_byte(0x06, 0);
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
	_delay_ms(10);
}

// --- DRIVER DHT11 (1-WIRE PROTOCOL) ---

uint8_t DHT11_Read(uint8_t *temp, uint8_t *hum) {
	uint8_t data[5] = {0, 0, 0, 0, 0};
	uint8_t i, j;

	DDRD |= (1 << DHT11_PIN);
	PORTD &= ~(1 << DHT11_PIN);
	_delay_ms(18);
	PORTD |= (1 << DHT11_PIN);
	_delay_us(30);
	DDRD &= ~(1 << DHT11_PIN);
	PORTD |= (1 << DHT11_PIN);

	_delay_us(40);
	if (PIND & (1 << DHT11_PIN)) return 0;
	_delay_us(80);
	if (!(PIND & (1 << DHT11_PIN))) return 0;
	while (PIND & (1 << DHT11_PIN));

	for (j = 0; j < 5; j++) {
		for (i = 0; i < 8; i++) {
			while (!(PIND & (1 << DHT11_PIN)));
			_delay_us(30);
			if (PIND & (1 << DHT11_PIN)) {
				data[j] |= (1 << (7 - i));
			}
			while (PIND & (1 << DHT11_PIN));
		}
	}

	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) == data[4]) {
		*hum  = data[0];
		*temp = data[2];
		return 1;
	}
	return 0;
}

// --- HARDWARE INIT ---

void Timer0_Init(void) {
	TCCR0A = (1 << WGM01);
	TCCR0B = (1 << CS01) | (1 << CS00);
	OCR0A = 249;
	TIMSK0 |= (1 << OCIE0A);
}

void Timer1_Init(void) {
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12);
	OCR1A = 6249;
	TIMSK1 |= (1 << OCIE1A);
}

void ADC_Init(void) {
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

void INT0_Init(void) {
	EICRA |= (1 << ISC01);
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

// --- TAREAS Y FSM ---

void Tarea_LED(void) {
	PORTB ^= (1 << LED_PIN);
}

void Tarea_Temperatura(void) {
	uint8_t t = 0, h = 0;
	if (DHT11_Read(&t, &h)) {
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
		id_estado_actual = ESTADO_ACTIVO;
		LCD_Clear();
	}
}

FuncionEstado fsm_estados[] = { estado_activo, estado_standby, estado_apagado };

int main(void) {
	DDRB |= (1 << LED_PIN) | (1 << BUZZER_PIN);
	DDRD &= ~(1 << BUTTON_PIN);
	PORTD |= (1 << BUTTON_PIN);

	LCD_Init();
	Timer0_Init();
	Timer1_Init();
	ADC_Init();
	INT0_Init();

	sei();

	while (1) {
		if (flag_evento_boton) {
			flag_evento_boton = false;
			PORTB |= (1 << BUZZER_PIN);
			_delay_ms(50);
			PORTB &= ~(1 << BUZZER_PIN);
		}

		fsm_estados[id_estado_actual]();
	}
}
