import serial
import re
import time
import threading
import collections
import matplotlib

# Forzar backend interactivo según el sistema
try:
    matplotlib.use('QtAgg')
except Exception:
    matplotlib.use('TkAgg')

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button

# =========================================================
# CONFIGURACIÓN
# =========================================================
PUERTO_COM = 'COM7
BAUDRATE = 9600

MAX_PUNTOS = 100
tiempos = collections.deque(maxlen=MAX_PUNTOS)
sp_datos = collections.deque(maxlen=MAX_PUNTOS)
ldr_datos = collections.deque(maxlen=MAX_PUNTOS)
pot_datos = collections.deque(maxlen=MAX_PUNTOS)
pwm_datos = collections.deque(maxlen=MAX_PUNTOS)

# Regex para extraer datos
patron = re.compile(r"SP:\s*(\d+),\s*LDR:\s*(\d+),\s*POT:\s*(\d+),\s*PWM:\s*(\d+),\s*DIR:\s*(\w+)")

sp_actual = 512
tiempo_inicio = time.time()
ser = None
lock = threading.Lock()
dir_motor_actual = "STOP"

# =========================================================
# HILO DE LECTURA SERIE (NO BLOQUEA LA GRÁFICA)
# =========================================================
def lectura_serie():
    global ser, dir_motor_actual
    try:
        ser = serial.Serial(PUERTO_COM, BAUDRATE, timeout=1)
        time.sleep(2)
        print(f"[OK] Conectado exitosamente al puerto {PUERTO_COM}")
    except Exception as e:
        print(f"[ERROR CRÍTICO] No se pudo abrir {PUERTO_COM}: {e}")
        print(" Asegúrate de cerrar el Monitor Serie de Arduino/Proteus.")
        return

    while ser and ser.is_open:
        try:
            linea = ser.readline().decode('utf-8', errors='ignore').strip()
            if not linea:
                continue

            # Muestra en la consola de Python exactamente lo que llega del micro
            print(f"[MCU -> PYTHON]: {linea}")

            coincidencia = patron.search(linea)
            if coincidencia:
                sp = int(coincidencia.group(1))
                ldr = int(coincidencia.group(2))
                pot = int(coincidencia.group(3))
                pwm = int(coincidencia.group(4))
                dir_m = coincidencia.group(5)

                t_actual = round(time.time() - tiempo_inicio, 1)

                with lock:
                    tiempos.append(t_actual)
                    sp_datos.append(sp)
                    ldr_datos.append(ldr)
                    pot_datos.append(pot)
                    pwm_datos.append(pwm)
                    dir_motor_actual = dir_m
            else:
                print(f"[ADVERTENCIA] Trama no coincide con la Regex: '{linea}'")

        except Exception as e:
            print(f"[ERROR LECTURA]: {e}")
            time.sleep(0.1)

# Iniciar el hilo de comunicación serie en segundo plano
hilo_serial = threading.Thread(target=lectura_serie, daemon=True)
hilo_serial.start()

# =========================================================
# CONFIGURACIÓN DE INTERFAZ GRÁFICA
# =========================================================
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
fig.canvas.manager.set_window_title('Laboratorio 3 - Monitoreo en Tiempo Real')
plt.subplots_adjust(bottom=0.18, hspace=0.3)

line_sp, = ax1.plot([], [], 'r--', label='Setpoint (SP)')
line_ldr, = ax1.plot([], [], 'g-', label='LDR (Iluminación Real)')
ax1.set_ylabel('Luz (0-1023)')
ax1.set_ylim(-10, 1035)
ax1.set_title('Monitoreo en Tiempo Real - Control de Iluminación y Persiana')
ax1.legend(loc='upper right')
ax1.grid(True)

line_pot, = ax2.plot([], [], 'b-', label='Posición Motor (POT)')
ax2.set_ylabel('Posición (0-1023)')
ax2.set_ylim(-10, 1035)
ax2.legend(loc='upper right')
ax2.grid(True)

line_pwm, = ax3.plot([], [], 'm-', label='PWM Aplicado')
ax3.set_xlabel('Tiempo (segundos)')
ax3.set_ylabel('PWM (0-255)')
ax3.set_ylim(-5, 260)
ax3.legend(loc='upper right')
ax3.grid(True)

texto_dir = ax3.text(0.02, 0.80, 'DIR: --', transform=ax3.transAxes,
                     bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

# =========================================================
# CONTROLES Y BOTONES
# =========================================================
def enviar_setpoint(nuevo_sp):
    global sp_actual
    sp_actual = max(0, min(1023, nuevo_sp))
    if ser and ser.is_open:
        try:
            comando = f"{sp_actual}\n".encode('utf-8')
            ser.write(comando)
            print(f"[PYTHON -> MCU]: Setpoint enviado: {sp_actual}")
        except Exception as e:
            print(f"[ERROR ENVÍO]: {e}")

def incrementar_sp(event=None):
    enviar_setpoint(sp_actual + 50)

def decrementar_sp(event=None):
    enviar_setpoint(sp_actual - 50)

def manejar_tecla(event):
    if event.key in ['+', 'u', 'U']:
        incrementar_sp()
    elif event.key in ['-', 'd', 'D']:
        decrementar_sp()

fig.canvas.mpl_connect('key_press_event', manejar_tecla)

ax_btn_dec = plt.axes([0.25, 0.03, 0.20, 0.05])
ax_btn_inc = plt.axes([0.55, 0.03, 0.20, 0.05])

btn_dec = Button(ax_btn_dec, '- SP (-50)')
btn_inc = Button(ax_btn_inc, '+ SP (+50)')

btn_dec.on_clicked(decrementar_sp)
btn_inc.on_clicked(incrementar_sp)

# =========================================================
# ACTUALIZACIÓN DE LA GRÁFICA
# =========================================================
def update_plot(frame):
    with lock:
        if len(tiempos) > 0:
            t_list = list(tiempos)
            line_sp.set_data(t_list, list(sp_datos))
            line_ldr.set_data(t_list, list(ldr_datos))
            line_pot.set_data(t_list, list(pot_datos))
            line_pwm.set_data(t_list, list(pwm_datos))
            texto_dir.set_text(f'DIR: {dir_motor_actual}')

            ax1.set_xlim(t_list[0], t_list[-1] + 0.5)

ani = FuncAnimation(fig, update_plot, interval=50, cache_frame_data=False)
plt.show()
