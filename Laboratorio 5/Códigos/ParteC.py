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
