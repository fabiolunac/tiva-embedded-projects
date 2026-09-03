## ADC implementation on Tiva C Series TM4C1294

- The main goal of the project is to develop code that can receive a extern signal, sample it at a specified sampling rate, and send it to a Python program.
- The requirements are to correctly configure the peripherals: UART, uDMA, Timer, and ADC.

### The project has two files:
- main.c with Tiva's configuration, developed on Code Composer Studio.
- main.py: program develop to receive the signal and analyze it.
