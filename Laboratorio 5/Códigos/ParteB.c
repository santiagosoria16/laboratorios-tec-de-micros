// Define la frecuencia de reloj del microcontrolador (16 MHz)
#define F_CPU 16000000UL
#define BAUD 9600 // UART
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

// Librerías principales de AVR C
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <avr/sleep.h> //  Modos Sleep
#include <avr/wdt.h>   //  Watchdog Timer
#include <avr/power.h> //  Power Reduction Register (PRR)

// --- DEFINICIÓN DE PINES DEL SISTEMA ---
#define LED_PIN              PB5   // Pin 13 Arduino (LED integrado)
#define BUZZER_PIN           PB0   // Pin 8 Arduino (Buzzer pasivo)
#define BUTTON_PIN           PD2   // Pin 2 Arduino (Entrada INT0 - Botón Navegación)
#define EMERGENCY_BUTTON_PIN PD3   // Pin 3 Arduino (Entrada INT1 - Botón Emergencia)

// Pines para las pruebas del WDT
#define WDT_TEST_PIN         PB1   // Pin 9 Arduino: conmuta de nivel en cada interrupción WDT
#define HANG_PIN             PD4   // Pin 4 Arduino (pull-up): a GND simula el bloqueo

// --- MAPEO Y REGISTROS DEL SENSOR DHT11 ---
#define DHT_PORT        PORTB
#define DHT_DDR         DDRB
#define DHT_PIN         PINB
#define DHT_BIT         PB4   // Pin 12 Arduino

// --- CONFIGURACIÓN DEL DISPLAY LCD I2C (EXPANSOR PCF8574) ---
#define I2C_ADDR        0x27
#define LCD_ENABLE      0x04
#define LCD_RS          0x01

// --- MÁQUINA DE ESTADOS FINITA (FSM) ---
typedef enum {
	ESTADO_ACTIVO = 0,
	ESTADO_STANDBY,
	ESTADO_APAGADO,
	ESTADO_EMERGENCIA
} EstadoSistema;

typedef void (*FuncionEstado)(void);
// --- BÚFERES CIRCULARES UART  ---
#define UART_BUFFER_SIZE 64

volatile char rx_buffer[UART_BUFFER_SIZE];
volatile uint8_t rx_head = 0;
volatile uint8_t rx_tail = 0;

volatile char tx_buffer[UART_BUFFER_SIZE];
volatile uint8_t tx_head = 0;
volatile uint8_t tx_tail = 0;

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
volatile bool flag_evento_emergencia = false;
volatile bool flag_wdt_despertar = false;

volatile uint16_t wdt_eventos = 0;        // Registro de eventos de despertar del WDT

volatile uint16_t valor_adc = 0;
volatile uint8_t dht_temp = 0;
volatile uint8_t dht_hum = 0;

volatile EstadoSistema id_estado_actual = ESTADO_ACTIVO;

uint8_t causa_reset_mcusr = 0;
volatile uint8_t lcd_backlight_mask = 0x08;

// --- DETECCIÓN ROBUSTA DE RESET POR WDT ---
uint8_t mcusr_mirror __attribute__((section(".noinit")));
uint32_t wdt_signature __attribute__((section(".noinit")));
#define WDT_MAGIC_KEY 0xDEADBEEF // detectar WDT reset tras bootloader

void get_mcusr(void) __attribute__((naked)) __attribute__((used)) __attribute__((section(".init3")));
void get_mcusr(void) {
	uint8_t mcusr_val = MCUSR;
	uint8_t r2_val;
	__asm__ __volatile__ ("mov %0, r2 \n" : "=r" (r2_val));
	mcusr_mirror = mcusr_val | r2_val;
	MCUSR = 0;
	wdt_disable(); // Deshabilitación inmediata
}

// --- DRIVER BÚFER CIRCULAR UART POR INTERRUPCIÓN ---
void USART_Init(unsigned int ubrr) {
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

ISR(USART_RX_vect) {
	char dato = UDR0;
	uint8_t next_head = (rx_head + 1) % UART_BUFFER_SIZE;
	if (next_head != rx_tail) {
		rx_buffer[rx_head] = dato;
		rx_head = next_head;
	}
}

ISR(USART_UDRE_vect) {
	if (tx_head != tx_tail) {
		UDR0 = tx_buffer[tx_tail];
		tx_tail = (tx_tail + 1) % UART_BUFFER_SIZE;
		} else {
		UCSR0B &= ~(1 << UDRIE0);
	}
}

bool USART_Available(void) {
	return (rx_head != rx_tail);
}

char USART_Read(void) {
	if (rx_head == rx_tail) return 0;
	char dato = rx_buffer[rx_tail];
	rx_tail = (rx_tail + 1) % UART_BUFFER_SIZE;
	return dato;
}

// --- GESTIÓN DEL REGISTRO POWER REDUCTION (PRR) ---
static void ADC_Apagar(void) {
	if (!(PRR & (1 << PRADC))) {
		while (ADCSRA & (1 << ADSC));
		ADCSRA &= ~(1 << ADEN);
	}
}

void PRR_Config_Activo(void) {
	PRR = (1 << PRSPI) | (1 << PRTIM2);
}

void PRR_Config_Standby(void) {
	ADC_Apagar();
	PRR = (1 << PRADC) | (1 << PRTIM1) | (1 << PRSPI) | (1 << PRTIM2);
}


void PRR_Config_Apagado(void) {
	ADC_Apagar();
	PRR = (1 << PRADC) | (1 << PRUSART0) | (1 << PRSPI) | (1 << PRTIM1) | (1 << PRTIM0) | (1 << PRTIM2) | (1 << PRTWI);
}

// --- GENERADOR DE TONOS PARA BUZZER ---
void Buzzer_Beep(void) { // Pitido corto de interacción normal
	for (uint16_t i = 0; i < 100; i++) {
		PORTB |= (1 << BUZZER_PIN);
		_delay_us(500);
		PORTB &= ~(1 << BUZZER_PIN);
		_delay_us(500);
	}
}

void Buzzer_Alarm(void) { // Alarma ruidosa para estado de emergencia
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
	TWBR = 72;
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

	lcd_send_byte(0x28, 0);
	lcd_send_byte(0x0C, 0);
	lcd_send_byte(0x01, 0);
	_delay_ms(2);
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

	DHT_DDR |= (1 << DHT_BIT);
	DHT_PORT &= ~(1 << DHT_BIT);
	_delay_ms(20);

	cli();

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

// --- CONFIGURACIÓN DEL WATCHDOG TIMER ---

// Habilita el WDT estrictamente en MODO RESET (2.0s, WDIE=0)
void WDT_Enable_Reset(void) {
	cli();
	wdt_reset();
	MCUSR &= ~(1 << WDRF);
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP1); // 2.0s, WDIE = 0
	sei();
}

// PRUEBA A: Habilita WDT en modo Interrupción pura a 8.0s para el modo APAGADO
void WDT_Init_Interrupt_8s(void) {
	MCUSR &= ~(1 << WDRF);
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
}

void WDT_Disable(void) {
	wdt_disable();
}

// SIMULACIÓN DE BLOQUEO: Pin 4 de Arduino a GND
void Simular_Bloqueo(void) {
	if (!(PIND & (1 << HANG_PIN))) { // Si Pin 4 pasa a GND
		wdt_signature = WDT_MAGIC_KEY;
		LCD_Clear();
		LCD_SetCursor(0, 0);
		LCD_Print("BLOQUEO DE CPU..");
		LCD_SetCursor(1, 0);
		LCD_Print("Desconecte PIN4");
		while (1) { } // Bucle infinito: al no llamar wdt_reset(), el WDT reinicia la MCU
	}
}


// --- CONFIGURACIÓN DE PERIFÉRICOS HARDWARE ---
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

void External_Interrupts_Init(void) {
	EICRA |= (1 << ISC01) | (1 << ISC11);
	EIMSK |= (1 << INT0) | (1 << INT1);
}

void Perifericos_Reanudar(void) {
	PRR_Config_Activo();
	USART_Init(MYUBRR);
	TWI_Init();
	ADC_Init();
	WDT_Enable_Reset();
}

void Lectura_Rapida_Despertar(void) {
	PRR &= ~(1 << PRADC);
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	valor_adc = ADC;
	ADCSRA &= ~(1 << ADEN);
	PRR |= (1 << PRADC);
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
	static uint32_t ultima_int0 = 0;
	if (milisegundos_totales - ultima_int0 > 150 || milisegundos_totales < ultima_int0) {
		flag_evento_boton = true;
		ultimo_evento_usuario = milisegundos_totales;
		ultima_int0 = milisegundos_totales;
	}
}

ISR(INT1_vect) {
	static uint32_t ultima_int1 = 0;
	if (milisegundos_totales - ultima_int1 > 150 || milisegundos_totales < ultima_int1) {
		flag_evento_emergencia = true;
		ultimo_evento_usuario = milisegundos_totales;
		ultima_int1 = milisegundos_totales;
	}
}

ISR(WDT_vect) {
	flag_wdt_despertar = true;
	wdt_eventos++;
	PORTB |= (1 << WDT_TEST_PIN);
	_delay_ms(100);
	PORTB &= ~(1 << WDT_TEST_PIN);
	// Solo re-habilitar WDIE en ESTADO_APAGADO
	if (id_estado_actual == ESTADO_APAGADO) {
		WDTCSR |= (1 << WDIE);
	}
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

