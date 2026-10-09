#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

// =========================================================
// PINES Y DEFINICIONES DE HARDWARE
// =========================================================
#define CALEFACTOR PB0       // Pin digital 8
#define VENTILADOR PB1       // Pin digital 9 - OC1A

// DHT11
#define DHT_PORT PORTC
#define DHT_DDR  DDRC
#define DHT_PIN  PINC
#define DHT_BIT  PC0         // A0

// LCD I2C
#define SDA PC4              // A4
#define SCL PC5              // A5
#define LCD_ADDR 0x27

// =========================================================
// LIMITES DEL PUNTO MEDIO
// =========================================================
#define PM_MAX_PERMITIDO 25
#define PM_MIN_PERMITIDO 10

// =========================================================
// VARIABLES GLOBALES
// =========================================================
volatile uint8_t flag_medir = 0;
volatile uint16_t contador_ms = 0;

uint8_t temperatura = 0;
uint8_t humedad = 0;
int8_t punto_medio = 20;

// Variables de estado de actuadores para telemetría
uint8_t estado_calefactor = 0; // 0: Apagado, 1: Encendido
uint8_t pwm_ventilador = 0;    // 0, 85, 170, 255

// =========================================================
// UART
// =========================================================
void uart_init(void) {
	uint16_t ubrr = (uint16_t)((F_CPU / (16UL * 9600UL)) - 1);
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_putchar(char c) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void uart_print(const char *texto) {
	while (*texto) {
		uart_putchar(*texto++);
	}
}

uint8_t uart_available(void) {
	return (UCSR0A & (1 << RXC0));
}

char uart_getchar(void) {
	while (!uart_available());
	return UDR0;
}

void mostrar_menu_uart(void) {
	uart_print("\r\n========================================\r\n");
	uart_print("       MENU PUNTO MEDIO (PM)\r\n");
	uart_print("========================================\r\n");
	uart_print(" + / u : Incrementar PM (+1 C)\r\n");
	uart_print(" - / d : Decrementar PM (-1 C)\r\n");
	uart_print(" m / M : Mostrar este menu\r\n");
	uart_print("----------------------------------------\r\n");
}

// =========================================================
// FUNCION AUXILIAR Y LECTURA DHT11
// =========================================================
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

// =========================================================
// I2C Y LCD
// =========================================================
void i2c_init(void) {
	TWSR = 0x00;
	TWBR = (uint8_t)(((F_CPU / 100000UL) - 16) / 2);
	DDRC &= ~((1 << SDA) | (1 << SCL));
	PORTC |= (1 << SDA) | (1 << SCL);
}

uint8_t i2c_start_timeout(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	uint16_t timer = 10000;
	while (!(TWCR & (1 << TWINT))) {
		if (--timer == 0) return 1;
	}
	return 0;
}

void i2c_stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	_delay_us(10);
}

void i2c_write_timeout(uint8_t data) {
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	uint16_t timer = 10000;
	while (!(TWCR & (1 << TWINT))) {
		if (--timer == 0) break;
	}
}

void lcd_write(uint8_t data) {
	if (i2c_start_timeout() == 0) {
		i2c_write_timeout(LCD_ADDR << 1);
		i2c_write_timeout(data);
		i2c_stop();
	}
}

void lcd_pulse(uint8_t data) {
	lcd_write(data | 0x04);
	_delay_us(1);
	lcd_write(data & ~0x04);
	_delay_us(50);
}

void lcd_nibble(uint8_t data, uint8_t rs) {
	uint8_t salida = data & 0xF0;
	if (rs) salida |= 0x01;
	salida |= 0x08;
	lcd_pulse(salida);
}

void lcd_send(uint8_t data, uint8_t rs) {
	lcd_nibble(data & 0xF0, rs);
	lcd_nibble((data << 4) & 0xF0, rs);
}

void lcd_command(uint8_t cmd) { lcd_send(cmd, 0); }
void lcd_data(uint8_t data) { lcd_send(data, 1); }

void lcd_clear(void) {
	lcd_command(0x01);
	_delay_ms(2);
}

void lcd_goto(uint8_t fila, uint8_t columna) {
	if (fila == 0) lcd_command(0x80 + columna);
	else if (fila == 1) lcd_command(0xC0 + columna);
}

void lcd_print_str(const char *str) {
	while (*str) lcd_data(*str++);
}

void lcd_init(void) {
	_delay_ms(50);
	lcd_nibble(0x30, 0); _delay_ms(5);
	lcd_nibble(0x30, 0); _delay_us(150);
	lcd_nibble(0x30, 0);
	lcd_nibble(0x20, 0);
	lcd_command(0x28);
	lcd_command(0x0C);
	lcd_clear();
}

void actualizar_pantalla_lcd_estado(uint8_t temp, int8_t pm, uint8_t status) {
	char line1[17];
	char line2[17];

	if (status == 0) snprintf(line1, sizeof(line1), "Temp: %d C       ", temp);
	else if (status == 99) snprintf(line1, sizeof(line1), "Temp: Esperando  ");
	else snprintf(line1, sizeof(line1), "Err DHT Cod: %d  ", status);

	snprintf(line2, sizeof(line2), "PM:   %d C       ", pm);

	lcd_goto(0, 0);
	lcd_print_str(line1);
	lcd_goto(1, 0);
	lcd_print_str(line2);
}

// =========================================================
// PWM TIMER1 & TIMER2
// =========================================================
void pwm_timer1_init(void) {
	DDRB |= (1 << VENTILADOR) | (1 << CALEFACTOR);
	PORTB &= ~(1 << CALEFACTOR);

	TCCR1A = (1 << COM1A1) | (1 << WGM10);
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1A = 0;
}

void set_ventilador_speed(uint8_t duty_cycle) {
	OCR1A = duty_cycle;
	pwm_ventilador = duty_cycle;
}

void timer2_init(void) {
	TCCR2A = (1 << WGM21);
	TCCR2B = (1 << CS22);
	OCR2A = 249;
	TIMSK2 |= (1 << OCIE2A);
}

ISR(TIMER2_COMPA_vect) {
	contador_ms++;
	if (contador_ms >= 5000) { // Cada 5 segundos
		contador_ms = 0;
		flag_medir = 1;
	}
}

// =========================================================
// CONTROL DE TEMPERATURA Y ENVÍO A PYTHON
// =========================================================
void procesar_control_temperatura(uint8_t temp) {
	char buffer_uart[120];

	int8_t t_min_calefactor = punto_medio - 5;
	int8_t t_max_ideal      = punto_medio + 5;
	int8_t t_max_low_fan   = punto_medio + 15;
	int8_t t_max_med_fan   = punto_medio + 25;

	if (temp <= t_min_calefactor) {
		PORTB |= (1 << CALEFACTOR);
		estado_calefactor = 1;
		set_ventilador_speed(0);
		} else if (temp > t_min_calefactor && temp <= t_max_ideal) {
		PORTB &= ~(1 << CALEFACTOR);
		estado_calefactor = 0;
		set_ventilador_speed(0);
		} else if (temp > t_max_ideal && temp <= t_max_low_fan) {
		PORTB &= ~(1 << CALEFACTOR);
		estado_calefactor = 0;
		set_ventilador_speed(85);
		} else if (temp > t_max_low_fan && temp <= t_max_med_fan) {
		PORTB &= ~(1 << CALEFACTOR);
		estado_calefactor = 0;
		set_ventilador_speed(170);
		} else {
		PORTB &= ~(1 << CALEFACTOR);
		estado_calefactor = 0;
		set_ventilador_speed(255);
	}

	// Trama de datos comprimida CSV para Python
	snprintf(buffer_uart, sizeof(buffer_uart), "DATA,%d,%d,%d,%d\r\n", temp, punto_medio, estado_calefactor, pwm_ventilador);
	uart_print(buffer_uart);
}

// =========================================================
// MAIN
// =========================================================
int main(void) {
	uart_init();
	i2c_init();
	lcd_init();
	pwm_timer1_init();
	timer2_init();

	lcd_goto(0, 0);
	lcd_print_str("Iniciando...");
	_delay_ms(2000);

	sei();

	mostrar_menu_uart();
	actualizar_pantalla_lcd_estado(temperatura, punto_medio, 99);

	while (1) {
		if (uart_available()) {
			char rx = uart_getchar();

			if (rx == '+' || rx == 'u' || rx == 'U') {
				if ((punto_medio + 1) <= PM_MAX_PERMITIDO) {
					punto_medio += 1;
					uart_print("[CONFIG] PM incrementado.\r\n");
					} else {
					uart_print("[ALERTA] PM limite maximo alcanzado.\r\n");
				}
				} else if (rx == '-' || rx == 'd' || rx == 'D') {
				if ((punto_medio - 1) >= PM_MIN_PERMITIDO) {
					punto_medio -= 1;
					uart_print("[CONFIG] PM decrementado.\r\n");
					} else {
					uart_print("[ALERTA] PM limite minimo alcanzado.\r\n");
				}
				} else if (rx == 'm' || rx == 'M') {
				mostrar_menu_uart();
			}

			actualizar_pantalla_lcd_estado(temperatura, punto_medio, 0);
		}

		if (flag_medir) {
			flag_medir = 0;
			uint8_t status = dht11_read(&humedad, &temperatura);

			if (status == 0) {
				procesar_control_temperatura(temperatura);
				} else {
				char err_msg[40];
				snprintf(err_msg, sizeof(err_msg), "[ERROR DHT11] Codigo: %d\r\n", status);
				uart_print(err_msg);
			}

			actualizar_pantalla_lcd_estado(temperatura, punto_medio, status);
		}
	}
	return 0;
}
