#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

// Dirección I2C del PCF8574 (Cambiar a 0x3F si la pantalla LCD no responde)
#define PCF8574_ADDR 0x27

// ==========================================
// DEFINICIÓN DE FRECUENCIAS (Hz)
// ==========================================
#define SIL 0

// Octava 2
#define B2  123

// Octava 3 - Bajos / Acompañamiento
#define C3  131
#define CS3 139
#define D3  147
#define DS3 156
#define EB3 156
#define E3  165
#define F3  175
#define FS3 185
#define G3  196
#define GS3 208
#define AB3 208
#define A3  220
#define AS3 233
#define BB3 233
#define B3  247

// Octava 4 - Melodía
#define C4  262
#define CS4 277
#define D4  294
#define DS4 311
#define EB4 311
#define E4  330
#define F4  349
#define FS4 370
#define G4  392
#define GS4 415
#define AB4 415
#define A4  440
#define AS4 466
#define BB4 466
#define B4  494

// Octava 5
#define C5  523
#define CS5 554
#define D5  587
#define DS5 622
#define EB5 622
#define E5  659
#define F5  698
#define FS5 740
#define G5  784
#define GS5 831
#define A5  880
#define AS5 932
#define BB5 932
#define B5  988

// Octava 6
#define C6  1047
#define D6  1175

#define FREQ_TO_ICR1(f) ((f > 0) ? ((F_CPU / (8UL * (f))) - 1) : 0)

// ==========================================
// ESTADOS Y MENÚ
// ==========================================
typedef enum {
	ESTADO_MENU,
	ESTADO_PIANO,
	ESTADO_CANCION
} EstadoSistema;

volatile EstadoSistema estadoActual = ESTADO_MENU;
volatile char comandoUART = 0;
volatile uint8_t nuevoComando = 0;

uint8_t opcionMenuActual = 0;
const char* opcionesMenu[] = {
	"1. Piano Manual",
	"2. Never Gonna",
	"3. Free Bird",
	"4. Wake Me Up"
};
#define TOTAL_OPCIONES 4

// ==========================================
// UART
// ==========================================
void UART_Init(unsigned int baud) {
	unsigned int ubrr = F_CPU / 16 / baud - 1;
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;

	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_TxChar(char c) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void UART_SendString(const char *str) {
	while (*str) {
		UART_TxChar(*str++);
	}
}

void UART_MostrarMenu(void) {
	UART_SendString("\r\n==================================================\r\n");
	UART_SendString("    PIANO ELECTRONICO POLIFONICO - ATmega328P     \r\n");
	UART_SendString("==================================================\r\n");
	UART_SendString(" Comandos UART Disponibles:\r\n");
	UART_SendString("  [1] Modo Piano Manual\r\n");
	UART_SendString("  [2] Cancion 1: Never Gonna Give You Up\r\n");
	UART_SendString("  [3] Cancion 2: Free Bird (Solo)\r\n");
	UART_SendString("  [4] Cancion 3: Wake Me Up When September Ends\r\n");
	UART_SendString("  [S] o [0] Detener cancion / Volver al Menu\r\n");
	UART_SendString("  [M] Mostrar este menu interactivo\r\n");
	UART_SendString("==================================================\r\n\r\n");
}

ISR(USART_RX_vect) {
	comandoUART = UDR0;
	nuevoComando = 1;
}

// ==========================================
// I2C CON TIMEOUT
// ==========================================
void I2C_Init(void) {
	TWSR = 0x00;
	TWBR = 0x48; // 100 kHz
	TWCR = (1 << TWEN);
}

uint8_t I2C_Start(void) {
	uint16_t timeout = 1000;
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT))) {
		if (--timeout == 0) return 0;
	}
	return 1;
}

void I2C_Stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	_delay_us(100);
}

uint8_t I2C_Write(uint8_t data) {
	uint16_t timeout = 1000;
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT))) {
		if (--timeout == 0) return 0;
	}
	return 1;
}

// ==========================================
// LCD 16x2 I2C
// ==========================================
#define LCD_BACKLIGHT 0x08

void LCD_I2C_WriteNibble(uint8_t nibble, uint8_t mode) {
	uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;

	if (!I2C_Start()) return;
	if (!I2C_Write(PCF8574_ADDR << 1)) { I2C_Stop(); return; }

	I2C_Write(data | 0x04);
	_delay_us(1);

	I2C_Write(data & ~0x04);
	_delay_us(50);

	I2C_Stop();
}

void LCD_I2C_Cmd(uint8_t cmd) {
	LCD_I2C_WriteNibble(cmd & 0xF0, 0);
	LCD_I2C_WriteNibble((cmd << 4) & 0xF0, 0);

	if (cmd == 0x01 || cmd == 0x02)
	_delay_ms(2);
}

void LCD_I2C_Char(uint8_t data) {
	LCD_I2C_WriteNibble(data & 0xF0, 1);
	LCD_I2C_WriteNibble((data << 4) & 0xF0, 1);
}

void LCD_I2C_Init(void) {
	_delay_ms(50);

	LCD_I2C_WriteNibble(0x30, 0);
	_delay_ms(5);

	LCD_I2C_WriteNibble(0x30, 0);
	_delay_us(200);

	LCD_I2C_WriteNibble(0x30, 0);
	LCD_I2C_WriteNibble(0x20, 0);

	LCD_I2C_Cmd(0x28);
	LCD_I2C_Cmd(0x0C);
	LCD_I2C_Cmd(0x06);
	LCD_I2C_Cmd(0x01);

	_delay_ms(2);
}

void LCD_I2C_SetCursor(uint8_t row, uint8_t col) {
	uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
	LCD_I2C_Cmd(addr);
}

void LCD_I2C_String(const char *str) {
	while (*str)
	LCD_I2C_Char((uint8_t)*str++);
}

void Mostrar_Menu_LCD(void) {
	LCD_I2C_Cmd(0x01);
	LCD_I2C_SetCursor(0, 0);
	LCD_I2C_String("MENU PRINCIPAL:");
	LCD_I2C_SetCursor(1, 0);
	LCD_I2C_String(">");
	LCD_I2C_String(opcionesMenu[opcionMenuActual]);
}

// ==========================================
// PWM / AUDIO
// ==========================================
void PWM_Init(void) {
	// Timer 1 - Melodía - PB1 / OC1A (Fast PWM con ICR1 como TOP)
	TCCR1A = (1 << COM1A1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);

	DDRB |= (1 << PB1);
	PORTB &= ~(1 << PB1);

	// Timer 2 - Bajo - PB3 / OC2A (CTC Mode, Toggle OC2A, Prescaler 256)
	TCCR2A = (1 << WGM21);
	TCCR2B = (1 << CS22) | (1 << CS21);

	DDRB |= (1 << PB3);
	PORTB &= ~(1 << PB3);
}

void Set_Tone_Buzzer1(uint16_t freq) {
	if (freq == 0) {
		TCCR1A &= ~(1 << COM1A1);
		PORTB &= ~(1 << PB1);
		} else {
		TCCR1A |= (1 << COM1A1);
		ICR1 = FREQ_TO_ICR1(freq);
		OCR1A = ICR1 / 2;
	}
}

void Set_Tone_Buzzer2(uint16_t freq) {
	if (freq == 0) {
		TCCR2A &= ~(1 << COM2A0);
		PORTB &= ~(1 << PB3);
		} else {
		TCCR2A |= (1 << COM2A0);
		uint32_t val = ((31250UL + (freq / 2)) / freq) - 1;
		if (val > 255) val = 255;
		OCR2A = (uint8_t)val;
		TCNT2 = 0;
	}
}

// ==========================================
// PULSADORES
// ==========================================
uint8_t Leer_Pulsadores(uint16_t *freq_out, char *nota_out) {
	if (!(PINB & (1 << PB0))) { *freq_out = C4; sprintf(nota_out, "DO (C4)"); return 1; }
	if (!(PINB & (1 << PB2))) { *freq_out = D4; sprintf(nota_out, "RE (D4)"); return 1; }
	if (!(PINB & (1 << PB4))) { *freq_out = E4; sprintf(nota_out, "MI (E4)"); return 1; }
	if (!(PINB & (1 << PB5))) { *freq_out = F4; sprintf(nota_out, "FA (F4)"); return 1; }
	if (!(PINC & (1 << PC0))) { *freq_out = G4; sprintf(nota_out, "SOL(G4)"); return 1; }
	if (!(PINC & (1 << PC1))) { *freq_out = A4; sprintf(nota_out, "LA (A4)"); return 1; }
	if (!(PINC & (1 << PC2))) { *freq_out = B4; sprintf(nota_out, "SI (B4)"); return 1; }
	if (!(PINC & (1 << PC3))) { *freq_out = C5; sprintf(nota_out, "DO (C5)"); return 1; }

	*freq_out = 0;
	return 0;
}

uint8_t Hay_Boton_Pulsado(void) {
	return (!(PINB & (1 << PB0)) || !(PINB & (1 << PB2)) ||
	!(PINB & (1 << PB4)) || !(PINB & (1 << PB5)) ||
	!(PINC & (1 << PC0)) || !(PINC & (1 << PC1)) ||
	!(PINC & (1 << PC2)) || !(PINC & (1 << PC3)));
}

void Esperar_Liberacion_Botones(void) {
	while (Hay_Boton_Pulsado()) {
		_delay_ms(10);
	}
	_delay_ms(40); // Debounce
}

// ============================================================================
// CANCIÓN 1 - NEVER GONNA GIVE YOU UP (Bb Mayor, 114 BPM)
// ============================================================================
const uint16_t mel_c1[] = {
	// "Never gonna give you up"
	F4, G4, BB4, G4, D5, D5, C5, SIL,
	// "Never gonna let you down"
	F4, G4, BB4, G4, C5, C5, BB4, A4, G4, SIL,
	// "Never gonna run around and desert you"
	F4, G4, BB4, G4, BB4, C5, A4, F4, SIL, F4, C5, BB4, SIL,
	// "Never gonna make you cry"
	F4, G4, BB4, G4, D5, D5, C5, SIL,
	// "Never gonna say goodbye"
	F4, G4, BB4, G4, F5, C5, BB4, A4, G4, SIL,
	// "Never gonna tell a lie and hurt you"
	F4, G4, BB4, G4, BB4, C5, A4, F4, SIL, F4, C5, BB4, SIL
};

const uint16_t bajo_c1[] = {
	// Eb -> F -> Dm
	EB3, EB3, EB3, EB3, F3, F3, D3, SIL,
	// Eb -> F -> Gm
	EB3, EB3, EB3, EB3, F3, F3, BB3, A3, G3, SIL,
	// Eb -> F -> Dm -> Gm -> Cm -> F -> Bb
	EB3, EB3, EB3, EB3, F3, F3, D3, G3, SIL, C3, F3, BB3, SIL,
	// Eb -> F -> Dm
	EB3, EB3, EB3, EB3, F3, F3, D3, SIL,
	// Eb -> F -> Dm -> Gm
	EB3, EB3, EB3, EB3, F3, F3, D3, G3, G3, SIL,
	// Eb -> F -> Dm -> Gm -> Cm -> F -> Bb
	EB3, EB3, EB3, EB3, F3, F3, D3, G3, SIL, C3, F3, BB3, SIL
};

const uint16_t dur_c1[] = {
	// Frase 1 (114 BPM)
	130, 130, 130, 130, 310, 310, 620, 120,
	// Frase 2
	130, 130, 130, 130, 310, 310, 210, 210, 520, 120,
	// Frase 3
	130, 130, 130, 130, 260, 260, 260, 260, 80, 260, 260, 620, 160,
	// Frase 4
	130, 130, 130, 130, 310, 310, 620, 120,
	// Frase 5
	130, 130, 130, 130, 310, 310, 210, 210, 520, 120,
	// Frase 6
	130, 130, 130, 130, 260, 260, 260, 260, 80, 260, 260, 620, 200
};

const uint8_t len_c1 = sizeof(mel_c1) / sizeof(mel_c1[0]);

// ============================================================================
// CANCIÓN 2 - FREE BIRD ("that funky solo bit", 143 BPM)
// ============================================================================
const uint16_t mel_c2[] = {
	// Anacrusa
	D5, E5,
	// Compás 2
	G5, E5, G5, E5, G5, E5, D5, B4,
	// Compás 3
	C5, D5, C5, B4, A4, G4, E4, G4,
	// Compás 4
	A4, SIL, D5, E5,
	// Compás 5
	G5, E5, G5, E5, G5, E5, D5, B4,
	// Compás 6
	C5, D5, C5, B4, A4, G4, A4, B4,
	// Compás 7-8
	G4, SIL,
	// Compás 9: Semicorcheas
	G5, E5, D5, B4, G5, E5, D5, B4,
	G5, E5, D5, B4, G5, E5, D5, B4,
	A5, G5, E5, D5, G5, E5, D5, B4,
	C5, D5, C5, B4, A4, G4, G4, SIL
};

const uint16_t bajo_c2[] = {
	SIL, SIL,
	G3, G3, G3, G3, G3, G3, G3, G3,
	BB3, BB3, BB3, BB3, C3, C3, C3, C3,
	C3, SIL, G3, G3,
	G3, G3, G3, G3, G3, G3, G3, G3,
	BB3, BB3, BB3, BB3, C3, C3, C3, C3,
	G3, SIL,
	G3, G3, G3, G3, G3, G3, G3, G3,
	BB3, BB3, BB3, BB3, BB3, BB3, BB3, BB3,
	C3, C3, C3, C3, C3, C3, C3, C3,
	C3, C3, C3, C3, G3, G3, G3, SIL
};

const uint16_t dur_c2[] = {
	// Corcheas a 143 BPM (~210 ms)
	210, 210,
	210, 210, 210, 210, 210, 210, 210, 210,
	210, 210, 210, 210, 210, 210, 210, 210,
	420, 210, 210, 210,
	210, 210, 210, 210, 210, 210, 210, 210,
	210, 210, 210, 210, 210, 210, 210, 210,
	420, 210,
	// Semicorcheas (~105 ms)
	105, 105, 105, 105, 105, 105, 105, 105,
	105, 105, 105, 105, 105, 105, 105, 105,
	105, 105, 105, 105, 105, 105, 105, 105,
	210, 210, 210, 210, 210, 210, 420, 210
};

const uint8_t len_c2 = sizeof(mel_c2) / sizeof(mel_c2[0]);

// ============================================================================
// CANCIÓN 3 - WAKE ME UP WHEN SEPTEMBER ENDS (G Mayor, 104 BPM)
// ============================================================================
const uint16_t mel_c3[] = {
	// "Summer has come and passed"
	D4, D4, D4, D4, D4, B4,
	// "the innocent can never last"
	D4, D4, D4, D4, D4, B4, A4, G4,
	// Interludio
	SIL, SIL,
	// "Wake me up..."
	SIL, B4, C5, B4,
	// "...when September ends"
	SIL, B4, C5, B4, A4, G4,
	// "Like my fathers come to pass"
	D4, D4, D4, D4, D4, B4,
	// "seven years has gone so fast"
	D4, D4, D4, D4, D4, B4, A4, G4,
	// Interludio
	SIL, SIL,
	// "Wake me up..."
	SIL, B4, C5, B4,
	// "...when September ends"
	SIL, B4, C5, B4, A4, G4
};

const uint16_t dur_c3[] = {
	288, 288, 288, 288, 288, 865,
	288, 288, 288, 288, 288, 288, 288, 577,
	1154, 1154,
	288, 288, 288, 1154,
	288, 288, 288, 288, 288, 1154,
	288, 288, 288, 288, 288, 865,
	288, 288, 288, 288, 288, 288, 288, 577,
	1154, 1154,
	288, 288, 288, 1154,
	288, 288, 288, 288, 288, 1731
};

const uint16_t bajo_c3[] = {
	G3,   // G5
	FS3,  // G5/F#
	E3,   // Em7
	D3,   // G5/D
	C3,   // C
	C3,   // Cm
	G3,   // G5
	FS3,  // G5/F#
	E3,   // Em7
	D3,   // G5/D
	C3,   // C
	C3,   // Cm
	G3    // Cierre
};

const uint16_t dur_bajo_c3[] = {
	2308, 2308, 2308, 2308,
	2018, 2308,
	2308, 2308, 2308, 2308,
	2018, 2308,
	2308
};

const uint8_t len_c3 = sizeof(mel_c3) / sizeof(mel_c3[0]);
const uint8_t len_bajo_c3 = sizeof(bajo_c3) / sizeof(bajo_c3[0]);

// ==========================================
// REPRODUCCIÓN
// ==========================================
void Reproducir_Cancion(const uint16_t *mel, const uint16_t *bajo, const uint16_t *dur, uint8_t len, const char *nombre) {
	Esperar_Liberacion_Botones();

	LCD_I2C_Cmd(0x01);
	LCD_I2C_SetCursor(0, 0);
	LCD_I2C_String("Reproduciendo:");
	LCD_I2C_SetCursor(1, 0);
	LCD_I2C_String(nombre);

	UART_SendString("\r\n[REPRODUCCION] ");
	UART_SendString(nombre);
	UART_SendString("\r\n");

	for (uint8_t i = 0; i < len; i++) {
		if (nuevoComando || Hay_Boton_Pulsado()) {
			nuevoComando = 0;
			UART_SendString("[UART/HW] Detencion solicitada.\r\n");
			break;
		}

		Set_Tone_Buzzer1(mel[i]);
		Set_Tone_Buzzer2(bajo[i]);

		uint16_t durTotal = dur[i];
		uint16_t durSonido = (durTotal > 30) ? (durTotal - 25) : durTotal;
		uint16_t durSilencio = durTotal - durSonido;

		for (uint16_t t = 0; t < durSonido; t += 10) {
			_delay_ms(10);
			if (nuevoComando || Hay_Boton_Pulsado()) break;
		}

		Set_Tone_Buzzer1(0);
		Set_Tone_Buzzer2(0);
		for (uint16_t t = 0; t < durSilencio; t += 10) {
			_delay_ms(10);
			if (nuevoComando || Hay_Boton_Pulsado()) break;
		}
	}

	Set_Tone_Buzzer1(0);
	Set_Tone_Buzzer2(0);
	Esperar_Liberacion_Botones();

	estadoActual = ESTADO_MENU;
	Mostrar_Menu_LCD();
}

void Reproducir_Cancion3(void) {
	Esperar_Liberacion_Botones();

	uint8_t i = 0, j = 0, bajoActivo = 1;
	uint16_t tiempoBajo = 0;

	LCD_I2C_Cmd(0x01);
	LCD_I2C_SetCursor(0, 0);
	LCD_I2C_String("Reproduciendo:");
	LCD_I2C_SetCursor(1, 0);
	LCD_I2C_String("3. Wake Me Up");

	UART_SendString("\r\n[REPRODUCCION] Wake Me Up When September Ends\r\n");

	while (i < len_c3) {
		if (nuevoComando || Hay_Boton_Pulsado()) {
			nuevoComando = 0;
			UART_SendString("[UART/HW] Detencion solicitada.\r\n");
			break;
		}

		Set_Tone_Buzzer1(mel_c3[i]);
		if (bajoActivo && j < len_bajo_c3) Set_Tone_Buzzer2(bajo_c3[j]);
		else Set_Tone_Buzzer2(0);

		uint16_t durTotal = dur_c3[i];
		uint16_t durSonido = (durTotal > 30) ? (durTotal - 25) : durTotal;
		uint16_t durSilencio = durTotal - durSonido;

		for (uint16_t t = 0; t < durSonido; t += 10) {
			_delay_ms(10);
			tiempoBajo += 10;

			if (bajoActivo && j < len_bajo_c3 && tiempoBajo >= dur_bajo_c3[j]) {
				tiempoBajo = 0;
				j++;
				if (j < len_bajo_c3) Set_Tone_Buzzer2(bajo_c3[j]);
				else { bajoActivo = 0; Set_Tone_Buzzer2(0); }
			}

			if (nuevoComando || Hay_Boton_Pulsado()) break;
		}

		Set_Tone_Buzzer1(0);
		for (uint16_t t = 0; t < durSilencio; t += 10) {
			_delay_ms(10);
			tiempoBajo += 10;

			if (bajoActivo && j < len_bajo_c3 && tiempoBajo >= dur_bajo_c3[j]) {
				tiempoBajo = 0;
				j++;
				if (j < len_bajo_c3) Set_Tone_Buzzer2(bajo_c3[j]);
				else { bajoActivo = 0; Set_Tone_Buzzer2(0); }
			}

			if (nuevoComando || Hay_Boton_Pulsado()) break;
		}

		if (nuevoComando || Hay_Boton_Pulsado()) break;
		i++;
	}

	Set_Tone_Buzzer1(0);
	Set_Tone_Buzzer2(0);
	Esperar_Liberacion_Botones();

	estadoActual = ESTADO_MENU;
	Mostrar_Menu_LCD();
}

void Ejecutar_Opcion(uint8_t opcion) {
	switch (opcion) {
		case 0:
		estadoActual = ESTADO_PIANO;
		LCD_I2C_Cmd(0x01);
		LCD_I2C_SetCursor(0, 0);
		LCD_I2C_String("Modo: Piano");
		UART_SendString("[FSM] Modo Piano Manual Activado.\r\n");
		break;
		case 1:
		estadoActual = ESTADO_CANCION;
		Reproducir_Cancion(mel_c1, bajo_c1, dur_c1, len_c1, "1. Never Gonna");
		break;
		case 2:
		estadoActual = ESTADO_CANCION;
		Reproducir_Cancion(mel_c2, bajo_c2, dur_c2, len_c2, "2. Free Bird");
		break;
		case 3:
		estadoActual = ESTADO_CANCION;
		Reproducir_Cancion3();
		break;
	}
}

// ==========================================
// PROGRAMA PRINCIPAL
// ==========================================
int main(void) {
	// Entradas con Pull-Up interno
	DDRB &= ~((1 << PB0) | (1 << PB2) | (1 << PB4) | (1 << PB5));
	PORTB |=  (1 << PB0) | (1 << PB2) | (1 << PB4) | (1 << PB5);

	DDRC &= ~((1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3));
	PORTC |=  (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3);

	// Inicializaciones
	I2C_Init();
	LCD_I2C_Init();
	PWM_Init();
	UART_Init(9600);
	sei();

	UART_MostrarMenu();
	Mostrar_Menu_LCD();

	uint16_t frecuencia = 0;
	char nombreNota[12];
	uint8_t notaPrevia = 0;
	uint8_t botonPresionadoAntes = 0;

	while (1) {
		// ----------------------------------
		// ATENCIÓN COMANDOS UART
		// ----------------------------------
		if (nuevoComando) {
			char cmd = comandoUART;
			nuevoComando = 0;

			if (cmd == '1') { Ejecutar_Opcion(0); }
			else if (cmd == '2') { Ejecutar_Opcion(1); }
			else if (cmd == '3') { Ejecutar_Opcion(2); }
			else if (cmd == '4') { Ejecutar_Opcion(3); }
			else if (cmd == 'S' || cmd == 's' || cmd == '0' || cmd == 'M' || cmd == 'm') {
				estadoActual = ESTADO_MENU;
				Set_Tone_Buzzer1(0);
				Set_Tone_Buzzer2(0);
				Mostrar_Menu_LCD();
				UART_SendString("[FSM] Retorno al MENU PRINCIPAL.\r\n");
			}
		}

		// ----------------------------------
		// MODO MENÚ
		// ----------------------------------
		if (estadoActual == ESTADO_MENU) {
			// PB0: Navegar opción
			if (!(PINB & (1 << PB0))) {
				if (!botonPresionadoAntes) {
					opcionMenuActual = (opcionMenuActual + 1) % TOTAL_OPCIONES;
					Mostrar_Menu_LCD();
					botonPresionadoAntes = 1;
				}
			}
			// PB2: Seleccionar opción
			else if (!(PINB & (1 << PB2))) {
				if (!botonPresionadoAntes) {
					botonPresionadoAntes = 1;
					Ejecutar_Opcion(opcionMenuActual);
				}
				} else {
				botonPresionadoAntes = 0;
			}
			_delay_ms(20);
		}

		// ----------------------------------
		// MODO PIANO MANUAL
		// ----------------------------------
		else if (estadoActual == ESTADO_PIANO) {
			if (Leer_Pulsadores(&frecuencia, nombreNota)) {
				Set_Tone_Buzzer1(frecuencia);
				Set_Tone_Buzzer2(0);

				if (!notaPrevia) {
					LCD_I2C_SetCursor(1, 0);
					LCD_I2C_String("Nota: ");
					LCD_I2C_String(nombreNota);

					UART_SendString("Nota: ");
					UART_SendString(nombreNota);
					UART_SendString("\r\n");
					notaPrevia = 1;
				}
				} else {
				Set_Tone_Buzzer1(0);
				Set_Tone_Buzzer2(0);

				if (notaPrevia) {
					LCD_I2C_SetCursor(1, 0);
					LCD_I2C_String("                ");
					notaPrevia = 0;
				}
			}
		}
	}
}
