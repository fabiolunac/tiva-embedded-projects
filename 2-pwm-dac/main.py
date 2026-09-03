import serial
import numpy as np
import matplotlib.pyplot as plt

####################### Configurar porta serial #######################

portal_serial = serial.Serial('COM5', baudrate=115200, timeout=1)

####################### Enviar vetor para o Tiva #######################

def send_int_vector(serialobj: serial.Serial, vector: np.array) -> None:    
    for value in vector:
        serialobj.write(np.float32(value).tobytes())

####################### Parâmetros PWM #######################

clk = 120_000_000
f_pwm = 20_000

clk_pwm = clk/64
load = clk_pwm/f_pwm - 1


####################### Gerar sinal ####################### 

f = 200
Fs = 10000
Nc = 2
N = int(Fs/f * Nc)

t = np.linspace(0, N, 100)

x = 0.5*np.sin(2*np.pi*t*f/Fs) + 1

duty = load * x/2

####################### Enviar o vetor para o Tiva #######################

send_int_vector(portal_serial, duty)
