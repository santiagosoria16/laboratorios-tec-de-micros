import matplotlib
import matplotlib.pyplot as plt

# Backend inline para evitar cierres o congelamientos en Spyder
try:
    matplotlib.use('module://matplotlib_inline.backend_inline')
except Exception:
    pass

import csv
import time
import serial
import numpy as np
import pandas as pd

# CONFIGURACIÓN

PUERTO_SERIE = 'COM5'
BAUD_RATE = 115200            # debe coincidir con el firmware (BAUD)
DURACION_MEDICION_S = 45
ARCHIVO_SALIDA = 'medicion_consumo_parteC.csv'

# Modo calibración: poné el shunt en CORTO (o sin carga), ponelo en True,
# corré el script y copiá el offset que imprime en V_OFFSET_SALIDA.
MODO_CALIBRACION = False
DURACION_CALIBRACION_S = 5

# Circuito analógico
R_SHUNT = 4.7                # Ω
GANANCIA_AV = 1.0 

# ADC (deben coincidir con el firmware)
VREF = 5.0                    # V. Medí el AVCC real con el multímetro (por USB suele ser 4.7-5.0 V)
                              # Si usás referencia interna en el firmware: VREF = 1.1
ADC_MAX = 1023.0
OVERSAMPLE = 16               # igual a OVERSAMPLE del firmware

V_OFFSET_SALIDA = 0.0001
# Estados (inicio_s, fin_s, nombre). AJUSTÁ con los tiempos REALES de tu secuencia.
ESTADOS = [
    (0.0, 10.0, "ACTIVO"),
    (10.0, 30.0, "STANDBY"),
    (30.0, DURACION_MEDICION_S, "APAGADO"),
]
COLORES = {"ACTIVO": "#ffd9d9", "STANDBY": "#fff2cc", "APAGADO": "#d9e8ff"}

VENTANA_FILTRO = 9            # muestras (a 100 Hz -> 90 ms)
VCC = 5.0                     # para la potencia

# FUNCIONES

def capturar(duracion_s):
    """Devuelve lista de (t_ms, suma_adc) leída del puerto serie."""
    muestras = []
    with serial.Serial(PUERTO_SERIE, BAUD_RATE, timeout=1) as ser:
        time.sleep(2)               # el Arduino se resetea al abrir el puerto
        ser.reset_input_buffer()
        ser.readline()              # descarta una línea posiblemente incompleta
        t0 = time.time()
        while (time.time() - t0) < duracion_s:
            linea = ser.readline().decode('ascii', errors='ignore').strip()
            partes = linea.split(',')
            if len(partes) != 2:
                continue
            try:
                t_ms = int(partes[0])
                suma = int(partes[1])
            except ValueError:
                continue
            if not (0 <= suma <= ADC_MAX * OVERSAMPLE):
                continue            # trama corrupta
            muestras.append((t_ms, suma))
    return muestras


def suma_a_voltaje(suma):
    cuentas = np.asarray(suma, dtype=float) / OVERSAMPLE
    return cuentas / ADC_MAX * VREF


def integrar(y, x):
    if hasattr(np, 'trapezoid'):
        return np.trapezoid(y, x)
    return np.trapz(y, x)

# MODO CALIBRACIÓN

if MODO_CALIBRACION:
    print(f"Calibrando offset durante {DURACION_CALIBRACION_S} s (shunt en corto / sin carga)...")
    m = capturar(DURACION_CALIBRACION_S)
    if not m:
        raise SystemExit("No llegaron datos. Revisá puerto y baudios.")
    v = suma_a_voltaje([s for _, s in m])
    print(f"Muestras: {len(v)}")
    print(f"V_OFFSET_SALIDA = {np.mean(v):.4f} V   (desvío: {np.std(v)*1000:.2f} mV)")
    raise SystemExit
