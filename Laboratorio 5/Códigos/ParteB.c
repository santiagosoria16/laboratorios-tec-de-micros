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

