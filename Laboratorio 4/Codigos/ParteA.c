#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// ==========================================
// ASIGNACIÓN DE PINES Y REGISTROS
// ==========================================
#define LED_VERDE_PIN   PD6
#define LED_ROJO_PIN    PD7
#define BUZZER_PIN      PB0

#define SPI_SS          PB2 // Pin 10
#define SPI_MOSI        PB3 // Pin 11
#define SPI_MISO        PB4 // Pin 12
#define SPI_SCK         PB5 // Pin 13
#define RC522_RST       PB1 // Pin 9

#define PCF8574_ADDR    (0x27 << 1) // Dirección I2C Pantalla LCD

#define LCD_RS          (1 << 0)
#define LCD_EN          (1 << 2)
#define LCD_BACKLIGHT   (1 << 3)

#define EEPROM_MAGIC_ADDR  0x03FF
#define EEPROM_MAGIC_VAL   0xA6 // Marca de actualización EEPROM

// ==========================================
// CONTROLADOR TWI (I2C) & PANTALLA LCD 16x2
// ==========================================
static uint8_t lcd_backlight_val = LCD_BACKLIGHT;

void TWI_Init(void) {
	TWSR = 0x00;
	TWBR = 72; // 100 kHz a 16 MHz
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

void LCD_I2C_WriteNibble(uint8_t data) {
	TWI_Start();
	TWI_Write(PCF8574_ADDR);
	TWI_Write(data | lcd_backlight_val | LCD_EN);
	_delay_us(1);
	TWI_Write((data | lcd_backlight_val) & ~LCD_EN);
	_delay_us(50);
	TWI_Stop();
}

void LCD_I2C_Send(uint8_t value, uint8_t mode) {
	uint8_t highnib = value & 0xF0;
	uint8_t lownib = (value << 4) & 0xF0;
	LCD_I2C_WriteNibble(highnib | mode);
	LCD_I2C_WriteNibble(lownib | mode);
}

void LCD_I2C_Cmd(uint8_t cmd) {
	LCD_I2C_Send(cmd, 0);
}

void LCD_I2C_Data(uint8_t data) {
	LCD_I2C_Send(data, LCD_RS);
}

void LCD_I2C_Init(void) {
	TWI_Init();
	_delay_ms(50);
	
	LCD_I2C_WriteNibble(0x30); _delay_ms(5);
	LCD_I2C_WriteNibble(0x30); _delay_us(150);
	LCD_I2C_WriteNibble(0x30); _delay_ms(1);
	LCD_I2C_WriteNibble(0x20); _delay_ms(1);

	LCD_I2C_Cmd(0x28); // 2 Líneas, matriz 5x8
	LCD_I2C_Cmd(0x0C); // Display ON, Cursor OFF
	LCD_I2C_Cmd(0x06); // Modo Incremento
	LCD_I2C_Cmd(0x01); // Limpiar Pantalla
	_delay_ms(2);
}

void LCD_I2C_SetCursor(uint8_t row, uint8_t col) {
	uint8_t offsets[] = {0x00, 0x40};
	LCD_I2C_Cmd(0x80 | (col + offsets[row]));
}

void LCD_I2C_Print(const char *str) {
	while (*str) {
		LCD_I2C_Data((uint8_t)*str++);
	}
}

void LCD_I2C_Clear(void) {
	LCD_I2C_Cmd(0x01);
	_delay_ms(2);
}

// ==========================================
// CONTROLADOR USART (TERMINAL SERIE / PICSIMLAB)
// ==========================================
void USART_Init(void) {
	uint16_t ubrr = 103; // 9600 baudios
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

bool Terminal_CheckCard(uint8_t *uid) {
	static char hexBuf[9] = {0};
	static uint8_t bufPos = 0;

	while (UCSR0A & (1 << RXC0)) {
		char c = UDR0;
		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
			hexBuf[bufPos++] = c;
			if (bufPos == 8) {
				for (uint8_t i = 0; i < 4; i++) {
					char byteStr[3] = { hexBuf[i*2], hexBuf[i*2+1], '\0' };
					uid[i] = (uint8_t)strtoul(byteStr, NULL, 16);
				}
				bufPos = 0;
				memset(hexBuf, 0, sizeof(hexBuf));
				return true;
			}
			} else if (c == '\r' || c == '\n') {
			bufPos = 0;
			memset(hexBuf, 0, sizeof(hexBuf));
		}
	}
	return false;
}

// ==========================================
// DRIVER RFID MFRC522 (OPTIMIZADO PROTOCOLO OFICIAL)
// ==========================================
void SPI_Init(void) {
	DDRB |= (1 << SPI_MOSI) | (1 << SPI_SCK) | (1 << SPI_SS) | (1 << RC522_RST);
	DDRB &= ~(1 << SPI_MISO);
	PORTB |= (1 << SPI_SS) | (1 << RC522_RST);
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0); // SPI Maestro, Fosc/16
}

uint8_t SPI_Transfer(uint8_t data) {
	SPDR = data;
	while (!(SPSR & (1 << SPIF)));
	return SPDR;
}

void RC522_WriteReg(uint8_t addr, uint8_t val) {
	PORTB &= ~(1 << SPI_SS);
	SPI_Transfer((addr << 1) & 0x7E);
	SPI_Transfer(val);
	PORTB |= (1 << SPI_SS);
}

uint8_t RC522_ReadReg(uint8_t addr) {
	uint8_t val;
	PORTB &= ~(1 << SPI_SS);
	SPI_Transfer(((addr << 1) & 0x7E) | 0x80);
	val = SPI_Transfer(0x00);
	PORTB |= (1 << SPI_SS);
	return val;
}

void RC522_Init(void) {
	PORTB &= ~(1 << RC522_RST);
	_delay_ms(10);
	PORTB |= (1 << RC522_RST);
	_delay_ms(50);

	RC522_WriteReg(0x01, 0x0F); // Soft Reset
	_delay_ms(50);

	RC522_WriteReg(0x2A, 0x8D);
	RC522_WriteReg(0x2B, 0x3E);
	RC522_WriteReg(0x2C, 0x00);
	RC522_WriteReg(0x2D, 0x30);
	RC522_WriteReg(0x15, 0x40);
	RC522_WriteReg(0x11, 0x3D);

	// Ganancia de Antena a 48 dB (Máxima Potencia para chips HW-126)
	RC522_WriteReg(0x26, 0x70);

	uint8_t txControl = RC522_ReadReg(0x14);
	if ((txControl & 0x03) != 0x03) {
		RC522_WriteReg(0x14, txControl | 0x03);
	}
}

bool RC522_RequestA(uint8_t *bufferATQA) {
	RC522_WriteReg(0x0D, 0x07);
	RC522_WriteReg(0x04, 0x7F);
	RC522_WriteReg(0x0A, 0x80);
	RC522_WriteReg(0x01, 0x00);

	RC522_WriteReg(0x09, 0x26); // REQA
	RC522_WriteReg(0x01, 0x0C); // Transceive
	RC522_WriteReg(0x0D, 0x87);

	_delay_ms(20);

	uint8_t irq = RC522_ReadReg(0x04);
	uint8_t error = RC522_ReadReg(0x06);

	if ((irq & 0x20) && !(error & 0x1B)) {
		uint8_t fifoLen = RC522_ReadReg(0x0A) & 0x7F;
		if (fifoLen >= 2) {
			bufferATQA[0] = RC522_ReadReg(0x09);
			bufferATQA[1] = RC522_ReadReg(0x09);
			return true;
		}
	}
	return false;
}

bool RC522_Anticoll(uint8_t *uid) {
	RC522_WriteReg(0x0D, 0x00);
	RC522_WriteReg(0x04, 0x7F);
	RC522_WriteReg(0x0A, 0x80);
	RC522_WriteReg(0x01, 0x00);

	RC522_WriteReg(0x09, 0x93);
	RC522_WriteReg(0x09, 0x20);
	RC522_WriteReg(0x01, 0x0C);
	RC522_WriteReg(0x0D, 0x80);

	_delay_ms(20);

	uint8_t irq = RC522_ReadReg(0x04);
	uint8_t error = RC522_ReadReg(0x06);

	if ((irq & 0x20) && !(error & 0x1B)) {
		uint8_t fifoLen = RC522_ReadReg(0x0A) & 0x7F;
		if (fifoLen >= 4) {
			for (uint8_t i = 0; i < 4; i++) {
				uid[i] = RC522_ReadReg(0x09);
			}
			return true;
		}
	}
	return false;
}

bool RC522_CheckCard(uint8_t *uid) {
	uint8_t atqa[2];
	if (RC522_RequestA(atqa)) {
		_delay_ms(5);
		if (RC522_Anticoll(uid)) {
			return true;
		}
	}
	return false;
}

// ==========================================
// TECLADO MATRICIAL 4x4 (GPIO)
// ==========================================
void Keypad_Init(void) {
	DDRC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3);
	PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3);

	DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
	PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
}

char Keypad_GetKey(void) {
	const char keymap[4][4] = {
		{'1', '2', '3', 'A'},
		{'4', '5', '6', 'B'},
		{'7', '8', '9', 'C'},
		{'*', '0', '#', 'D'}
	};

	const uint8_t rowPins[4] = {PC0, PC1, PC2, PC3};
	const uint8_t colPins[4] = {PD2, PD3, PD4, PD5};

	for (uint8_t r = 0; r < 4; r++) {
		PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3);
		PORTC &= ~(1 << rowPins[r]);
		_delay_us(10);

		for (uint8_t c = 0; c < 4; c++) {
			if (!(PIND & (1 << colPins[c]))) {
				_delay_ms(20); // Anti-rebote
				while (!(PIND & (1 << colPins[c])));
				return keymap[r][c];
			}
		}
	}
	return '\0';
}

// ==========================================
// INDICADORES SONOROS Y VISUALES
// ==========================================
void Beep(uint16_t duration_ms, uint16_t freq_hz) {
	uint32_t delay_us = 1000000UL / (2 * freq_hz);
	uint32_t cycles = (uint32_t)duration_ms * 1000UL / (2 * delay_us);
	for (uint32_t i = 0; i < cycles; i++) {
		PORTB |= (1 << BUZZER_PIN);
		for (uint32_t d = 0; d < delay_us / 10; d++) _delay_us(10);
		PORTB &= ~(1 << BUZZER_PIN);
		for (uint32_t d = 0; d < delay_us / 10; d++) _delay_us(10);
	}
}

void AnimacionExito(void) {
	PORTD |= (1 << LED_VERDE_PIN);
	Beep(150, 2000);
	_delay_ms(200);
	PORTD &= ~(1 << LED_VERDE_PIN);
}

void AnimacionError(void) {
	PORTD |= (1 << LED_ROJO_PIN);
	Beep(400, 500);
	_delay_ms(200);
	PORTD &= ~(1 << LED_ROJO_PIN);
}

// ==========================================
// MEMORIA EEPROM CON UIDS REALES OBTENIDOS
// ==========================================
typedef struct {
	uint8_t uid[4];
	char pin[4];
	uint32_t saldo;
} Cuenta_t;

#define CANT_CUENTAS 3

void GuardarCuentaEEPROM(uint8_t index, const Cuenta_t *cta) {
	uint16_t addr = index * sizeof(Cuenta_t);
	eeprom_update_block((const void*)cta, (void*)addr, sizeof(Cuenta_t));
}

void CargarCuentaEEPROM(uint8_t index, Cuenta_t *cta) {
	uint16_t addr = index * sizeof(Cuenta_t);
	eeprom_read_block((void*)cta, (const void*)addr, sizeof(Cuenta_t));
}

void InicializarEEPROMDefecto(void) {
	// SE CARGAN LOS 3 UIDS OBTENIDOS POR EL ESCÁNER:
	Cuenta_t iniciales[CANT_CUENTAS] = {
		{ {0x0A, 0x98, 0xBD, 0x02}, {'1', '2', '3', '4'}, 5000 },
		{ {0x43, 0x5F, 0x1A, 0x2D}, {'4', '3', '2', '1'}, 12000 },
		{ {0xF3, 0xF6, 0x17, 0x29}, {'1', '1', '2', '2'}, 8500 }
	};
	for (uint8_t i = 0; i < CANT_CUENTAS; i++) {
		GuardarCuentaEEPROM(i, &iniciales[i]);
	}
}

void AutoInicializarEEPROM(void) {
	uint8_t magic = eeprom_read_byte((const uint8_t*)EEPROM_MAGIC_ADDR);
	if (magic != EEPROM_MAGIC_VAL) {
		InicializarEEPROMDefecto();
		eeprom_write_byte((uint8_t*)EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VAL);
	}
}

int8_t BuscarCuentaPorUID(const uint8_t *uid) {
	Cuenta_t cta;
	for (uint8_t i = 0; i < CANT_CUENTAS; i++) {
		CargarCuentaEEPROM(i, &cta);
		if (memcmp(cta.uid, uid, 4) == 0) {
			return i;
		}
	}
	return -1;
}

bool VerificarPIN(uint8_t index, const char *pinIngresado) {
	Cuenta_t cta;
	CargarCuentaEEPROM(index, &cta);
	return (strncmp(cta.pin, pinIngresado, 4) == 0);
}

// ==========================================
// MÁQUINA DE ESTADOS FINITOS (FSM)
// ==========================================
typedef enum {
	ST_ESPERA_TARJETA,
	ST_INGRESO_PIN,
	ST_MENU_PRINCIPAL,
	ST_CONSULTA_SALDO,
	ST_INGRESO_RETIRO,
	ST_INGRESO_DEPOSITO,
	ST_BLOQUEO
} EstadoFSM_t;

int main(void) {
	DDRD |= (1 << LED_VERDE_PIN) | (1 << LED_ROJO_PIN);
	DDRB |= (1 << BUZZER_PIN);
	PORTD &= ~((1 << LED_VERDE_PIN) | (1 << LED_ROJO_PIN));
	PORTB &= ~(1 << BUZZER_PIN);

	SPI_Init();
	RC522_Init();
	USART_Init();
	LCD_I2C_Init();
	Keypad_Init();

	// Graba la EEPROM automáticamente al iniciar
	AutoInicializarEEPROM();

	LCD_I2C_Clear();
	LCD_I2C_SetCursor(0, 0);
	LCD_I2C_Print("Hola");
	LCD_I2C_SetCursor(1, 0);
	LCD_I2C_Print(" Sistema Listo  ");
	_delay_ms(1500);
	LCD_I2C_Clear();

	EstadoFSM_t estado = ST_ESPERA_TARJETA;
	int8_t cuentaActiva = -1;
	uint8_t intentosPin = 0;
	char buffer[10] = {0};
	uint8_t bufIdx = 0;
	uint8_t uidLeido[4] = {0};

	while (1) {
		char tecla = Keypad_GetKey();

		switch (estado) {
			case ST_ESPERA_TARJETA:
			LCD_I2C_SetCursor(0, 0);
			LCD_I2C_Print("  CAJERO AUTOM. ");
			LCD_I2C_SetCursor(1, 0);
			LCD_I2C_Print("Acerque Tarjeta ");

			// Detecta tanto tarjetas físicas reales como entrada por terminal serie
			if (RC522_CheckCard(uidLeido) || Terminal_CheckCard(uidLeido)) {
				cuentaActiva = BuscarCuentaPorUID(uidLeido);
				if (cuentaActiva != -1) {
					intentosPin = 0;
					bufIdx = 0;
					memset(buffer, 0, sizeof(buffer));
					LCD_I2C_Clear();
					estado = ST_INGRESO_PIN;
					} else {
					LCD_I2C_Clear();
					LCD_I2C_SetCursor(0, 0);
					LCD_I2C_Print("Tarjeta no      ");
					LCD_I2C_SetCursor(1, 0);
					LCD_I2C_Print("Registrada!     ");
					AnimacionError();
					_delay_ms(1500);
					LCD_I2C_Clear();
				}
			}
			break;

			case ST_INGRESO_PIN:
			LCD_I2C_SetCursor(0, 0);
			LCD_I2C_Print("Ingrese PIN:    ");
			LCD_I2C_SetCursor(1, 0);
			for (uint8_t i = 0; i < 4; i++) {
				if (i < bufIdx) LCD_I2C_Data('*');
				else LCD_I2C_Data(' ');
			}
			LCD_I2C_Print("            ");

			if (tecla != '\0') {
				if (tecla >= '0' && tecla <= '9' && bufIdx < 4) {
					buffer[bufIdx++] = tecla;
					Beep(50, 1000);
					} else if (tecla == '*' && bufIdx > 0) {
					bufIdx--;
					buffer[bufIdx] = '\0';
					} else if (tecla == '#') {
					if (bufIdx == 4) {
						if (VerificarPIN(cuentaActiva, buffer)) {
							AnimacionExito();
							LCD_I2C_Clear();
							estado = ST_MENU_PRINCIPAL;
							} else {
							intentosPin++;
							AnimacionError();
							if (intentosPin >= 3) {
								estado = ST_BLOQUEO;
								} else {
								LCD_I2C_Clear();
								LCD_I2C_SetCursor(0, 0);
								LCD_I2C_Print("PIN Incorrecto ");
								LCD_I2C_SetCursor(1, 0);
								LCD_I2C_Print("Intentos res: ");
								LCD_I2C_Data('0' + (3 - intentosPin));
								_delay_ms(1500);
								bufIdx = 0;
								memset(buffer, 0, sizeof(buffer));
								LCD_I2C_Clear();
							}
						}
					}
				}
			}
			break;

			case ST_MENU_PRINCIPAL:
			LCD_I2C_SetCursor(0, 0);
			LCD_I2C_Print("1:Saldo 2:Retir ");
			LCD_I2C_SetCursor(1, 0);
			LCD_I2C_Print("3:Deposi 4:Salir");

			if (tecla != '\0') {
				Beep(50, 1000);
				bufIdx = 0;
				memset(buffer, 0, sizeof(buffer));
				LCD_I2C_Clear();

				if (tecla == '1') estado = ST_CONSULTA_SALDO;
				else if (tecla == '2') estado = ST_INGRESO_RETIRO;
				else if (tecla == '3') estado = ST_INGRESO_DEPOSITO;
				else if (tecla == '4') {
					cuentaActiva = -1;
					estado = ST_ESPERA_TARJETA;
				}
			}
			break;

			case ST_CONSULTA_SALDO: {
				Cuenta_t cta;
				CargarCuentaEEPROM(cuentaActiva, &cta);

				LCD_I2C_SetCursor(0, 0);
				LCD_I2C_Print("Saldo Actual:   ");
				LCD_I2C_SetCursor(1, 0);
				char strSaldo[16];
				sprintf(strSaldo, "$%lu              ", cta.saldo);
				LCD_I2C_Print(strSaldo);

				if (tecla != '\0') {
					Beep(50, 1000);
					LCD_I2C_Clear();
					estado = ST_MENU_PRINCIPAL;
				}
				break;
			}

			case ST_INGRESO_RETIRO:
			LCD_I2C_SetCursor(0, 0);
			LCD_I2C_Print("Monto Retiro:   ");
			LCD_I2C_SetCursor(1, 0);
			char strRetiro[16];
			sprintf(strRetiro, "$%s              ", buffer);
			LCD_I2C_Print(strRetiro);

			if (tecla != '\0') {
				if (tecla >= '0' && tecla <= '9' && bufIdx < 6) {
					buffer[bufIdx++] = tecla;
					buffer[bufIdx] = '\0';
					Beep(50, 1000);
					} else if (tecla == '*' && bufIdx > 0) {
					bufIdx--;
					buffer[bufIdx] = '\0';
					} else if (tecla == '#') {
					uint32_t monto = atol(buffer);
					Cuenta_t cta;
					CargarCuentaEEPROM(cuentaActiva, &cta);

					if (monto > 0 && cta.saldo >= monto) {
						cta.saldo -= monto;
						GuardarCuentaEEPROM(cuentaActiva, &cta);
						AnimacionExito();
						LCD_I2C_Clear();
						LCD_I2C_SetCursor(0, 0);
						LCD_I2C_Print("Retiro Exitoso ");
						_delay_ms(1500);
						} else {
						AnimacionError();
						LCD_I2C_Clear();
						LCD_I2C_SetCursor(0, 0);
						LCD_I2C_Print("Saldo Insufic. ");
						_delay_ms(1500);
					}
					LCD_I2C_Clear();
					estado = ST_MENU_PRINCIPAL;
				}
			}
			break;

			case ST_INGRESO_DEPOSITO:
			LCD_I2C_SetCursor(0, 0);
			LCD_I2C_Print("Monto Deposito: ");
			LCD_I2C_SetCursor(1, 0);
			char strDep[16];
			sprintf(strDep, "$%s              ", buffer);
			LCD_I2C_Print(strDep);

			if (tecla != '\0') {
				if (tecla >= '0' && tecla <= '9' && bufIdx < 6) {
					buffer[bufIdx++] = tecla;
					buffer[bufIdx] = '\0';
					Beep(50, 1000);
					} else if (tecla == '*' && bufIdx > 0) {
					bufIdx--;
					buffer[bufIdx] = '\0';
					} else if (tecla == '#') {
					uint32_t monto = atol(buffer);
					if (monto > 0) {
						Cuenta_t cta;
						CargarCuentaEEPROM(cuentaActiva, &cta);
						cta.saldo += monto;
						GuardarCuentaEEPROM(cuentaActiva, &cta);
						AnimacionExito();
						LCD_I2C_Clear();
						LCD_I2C_SetCursor(0, 0);
						LCD_I2C_Print("Deposito Exitoso");
						_delay_ms(1500);
						} else {
						AnimacionError();
					}
					LCD_I2C_Clear();
					estado = ST_MENU_PRINCIPAL;
				}
			}
			break;

			case ST_BLOQUEO:
			LCD_I2C_Clear();
			LCD_I2C_SetCursor(0, 0);
			LCD_I2C_Print("SISTEMA BLOQUEADO");
			LCD_I2C_SetCursor(1, 0);
			LCD_I2C_Print("3 INTENTOS FALL.");

			for (uint8_t i = 0; i < 10; i++) {
				PORTD |= (1 << LED_ROJO_PIN);
				Beep(250, 1500);
				PORTD &= ~(1 << LED_ROJO_PIN);
				_delay_ms(250);
			}

			cuentaActiva = -1;
			intentosPin = 0;
			LCD_I2C_Clear();
			estado = ST_ESPERA_TARJETA;
			break;
		}
		_delay_ms(10);
	}
	return 0;
}
