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
