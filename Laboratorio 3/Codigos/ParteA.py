import serial
import re
import time
import matplotlib.pyplot as plt
from matplotlib.widgets import Button

# =========================================================
# CONFIGURACIÓN
# =========================================================
PUERTO_COM = 'COM5'
BAUDRATE = 9600

tiempos = []
temperaturas = []
puntos_medios = []
calefactores = []
ventiladores = []

patron = re.compile(r"\[SISTEMA\] Temp:\s*(\d+)\s*C\s*\|\s*Accion:\s*(.*?)\s*\|\s*PM:\s*(\d+)\s*C")

# Configuración de ventana interactiva
plt.ion()
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)
fig.canvas.manager.set_window_title('Monitoreo del Sistema de Temperatura')
plt.subplots_adjust(bottom=0.15)  # Espacio para los botones

tiempo_inicio = time.time()

# =========================================================
# FUNCIONES PARA ENVIAR COMANDOS AL ATMEGA328P
# =========================================================
def incrementar_pm(event=None):
    if 'ser' in globals() and ser.is_open:
        ser.write(b'+')
        print("[PYTHON -> MCU]: Comando enviado: + (Incrementar PM)")

def decrementar_pm(event=None):
    if 'ser' in globals() and ser.is_open:
        ser.write(b'-')
        print("[PYTHON -> MCU]: Comando enviado: - (Decrementar PM)")

# Captura de teclas cuando la ventana del gráfico está enfocada
def manejar_tecla(event):
    if event.key in ['+', 'u', 'U']:
        incrementar_pm()
    elif event.key in ['-', 'd', 'D']:
        decrementar_pm()

fig.canvas.mpl_connect('key_press_event', manejar_tecla)

# Creación de botones gráficos
ax_btn_dec = plt.axes([0.25, 0.03, 0.20, 0.05])
ax_btn_inc = plt.axes([0.55, 0.03, 0.20, 0.05])

btn_dec = Button(ax_btn_dec, '- PM (-1°C)')
btn_inc = Button(ax_btn_inc, '+ PM (+1°C)')

btn_dec.on_clicked(decrementar_pm)
btn_inc.on_clicked(incrementar_pm)

# =========================================================
# BUCLE PRINCIPAL DE LECTURA Y GRAFICADO
# =========================================================
try:
    ser = serial.Serial(PUERTO_COM, BAUDRATE, timeout=1)
    print(f"Conectado exitosamente al puerto {PUERTO_COM}")
    time.sleep(2)

    while True:
        if ser.in_waiting > 0:
            linea = ser.readline().decode('utf-8', errors='ignore').strip()
            
            coincidencia = patron.search(linea)
            if coincidencia:
                temp = int(coincidencia.group(1))
                accion = coincidencia.group(2)
                pm = int(coincidencia.group(3))

                t_actual = round(time.time() - tiempo_inicio, 1)

                calef_val = 100 if "CALEFACTOR" in accion else 0
                if "BAJA" in accion:
                    fan_val = 33
                elif "MEDIA" in accion:
                    fan_val = 66
                elif "ALTA" in accion:
                    fan_val = 100
                else:
                    fan_val = 0

                tiempos.append(t_actual)
                temperaturas.append(temp)
                puntos_medios.append(pm)
                calefactores.append(calef_val)
                ventiladores.append(fan_val)

                # Subgráfica 1: Temperatura y Rango Ideal
                ax1.clear()
                ax1.plot(tiempos, temperaturas, 'r-o', label='Temperatura Medida (°C)')
                ax1.plot(tiempos, puntos_medios, 'b--', label='Punto Medio (°C)')
                
                rango_inf = [p - 5 for p in puntos_medios]
                rango_sup = [p + 5 for p in puntos_medios]
                ax1.fill_between(tiempos, rango_inf, rango_sup, color='green', alpha=0.15, label='Rango Ideal (PM ± 5°C)')

                ax1.set_ylabel('Temperatura (°C)')
                ax1.set_title('Monitoreo en Tiempo Real - Problema A')
                ax1.legend(loc='upper left')
                ax1.grid(True)

                # Subgráfica 2: Actuadores
                ax2.clear()
                ax2.step(tiempos, calefactores, color='orange', where='post', label='Calefactor (%)')
                ax2.plot(tiempos, ventiladores, color='green', marker='s', label='Ventilador (%)')

                ax2.set_xlabel('Tiempo (segundos)')
                ax2.set_ylabel('Actuadores (%)')
                ax2.set_ylim(-5, 110)
                ax2.legend(loc='upper left')
                ax2.grid(True)

            else:
                print("[MCU]:", linea)

        plt.pause(0.05)

except serial.SerialException as e:
    print(f"Error al abrir el puerto {PUERTO_COM}: {e}")
except KeyboardInterrupt:
    print("\nMonitoreo detenido por el usuario.")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Puerto serie cerrado.")
