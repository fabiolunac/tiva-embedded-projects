import serial
import numpy as np
import matplotlib.pyplot as plt

# # Configurar a porta serial 
porta_serial = serial.Serial('COM5', baudrate=115200, timeout=1)

Nc = 4 #numero de ciclos do sinal

Fs = 10000
f_signal = 200
f_test = 200

N = int((Fs/f_signal)*Nc) #numero de pontos do buffer

ADC_factor = int(4095/3.3)

def send_int_to_uart(serialobj: serial.Serial, value: int, num_bytes: int, byteorder: str) -> None:
    bytes_to_send = value.to_bytes(num_bytes, byteorder, signed=True)
    serialobj.write(bytes_to_send)


def read_int_vector(serialobj: serial.Serial, num_elements: int) -> np.ndarray:
    signal = []
    while len(signal) < num_elements:
        data = serialobj.read(4)

        if len(data) == 4:
            int_value = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24)
            signal.append(int_value)

        else:
            print('Esperando dados')

    return signal

def signal_FFT(signal: int) -> None:
    y = np.array(y) / ADC_factor

    Y = np.fft.fft(y)

    y_fft_magnitude = np.abs(Y)[:N // 2] 
    freqs = np.fft.fftfreq(N, 1 / Fs)[:N // 2]  

    plt.figure()
    plt.subplot(211)
    plt.plot(y)
    plt.grid()
    plt.title(f'Sinal Senoidal, f = {f_test} Hz')
    plt.ylabel("Amplitude (V)")

    plt.subplot(212)
    plt.stem(freqs, y_fft_magnitude*2/N)
    plt.axvline(x = f_test, color = 'red', linestyle = '--', label = f'Frequência do Sinal, f = {f_test} Hz')
    plt.grid()
    plt.title("FFT")
    plt.legend()
    plt.xlabel("Frequência (Hz)")
    plt.ylabel("Magnitude")

    plt.tight_layout()
    plt.show()


while True:
    try:
        x = input("1 - Iniciar Transferência\n0 - Não iniciar: ")

        if x == '0':
            send_int_to_uart(porta_serial, int(x), 4, 'little')
            print('Encerrando...\n')
            break

        elif x == '1':
            send_int_to_uart(porta_serial, int(x), 4, 'little')
            y = read_int_vector(porta_serial, N)

            signal_FFT(y)

        else:
            print('Comando inválido!\nDigite 1 para iniciar ou 0 para sair\n')

    except ValueError:
        print("Digite uma entrada correta")
