# 1 - Data Acquisition

Módulo de aquisição de dados com o Tiva C Series TM4C1294 atuando como servidor de amostragem para uma aplicação de análise em Python.

## Como funciona

- **Amostragem determinística**: o ADC0 é disparado por um **trigger de Timer0** (`ADC_TRIGGER_TIMER`) configurado para 10 kHz, em vez de polling, garantindo período de amostragem constante.
- **Buffer circular por interrupção**: cada amostra convertida é lida na interrupção do ADC (`ADCSeqHandler`) e armazenada em `signal[]`. Ao encher o buffer (`SIGNAL_SIZE = 200` amostras — 4 ciclos de um sinal de referência), o ADC é desabilitado e o buffer é enviado.
- **Transferência por µDMA**: o envio do buffer pela UART0 (115200 bps) usa o canal `UDMA_CHANNEL_UART0TX`, liberando a CPU durante a transmissão. A recepção do comando de start/stop também é feita via uDMA (`UDMA_CHANNEL_UART0RX`).
- **Handshake simples**: o firmware fica em loop lendo uma flag de 4 bytes vinda da UART; ao receber `1`, rearma o ADC e a interrupção para capturar e enviar o próximo buffer.

## Arquivos

| Arquivo | Descrição |
|---|---|
| [main.c](main.c) | Firmware do Tiva: configuração de ADC, Timer, UART e uDMA, e lógica de captura/envio do buffer. |
| [main.py](main.py) | Cliente Python: envia o comando de start/stop pela serial, recebe o vetor de amostras e plota o sinal e sua FFT. |

## Como executar

1. Compile e grave [main.c](main.c) no Tiva via Code Composer Studio (TivaWare Peripheral Driver Library).
2. Conecte o sinal de entrada ao canal `ADC0 CH0` (pino `PE3`) — o filtro anti-aliasing Sallen-Key (500 Hz de corte) deve preceder essa entrada.
3. Ajuste a porta serial em [main.py](main.py) (`porta_serial = serial.Serial('COM5', ...)`) e execute o script.
4. Digite `1` para iniciar uma captura (o script lê o buffer e plota o sinal no tempo e sua FFT) ou `0` para encerrar.

## Parâmetros principais

- Frequência de amostragem: 10 kHz
- Clock do sistema: 120 MHz (PLL a partir de cristal de 25 MHz)
- Baud rate UART: 115200 bps
