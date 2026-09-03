# 2 - PWM DAC

Conversor digital-analógico por modulação PWM, já que o TM4C1294 não possui DAC integrado. Um sinal senoidal gerado em Python é transmitido ao Tiva, que reproduz a forma de onda modulando a razão cíclica de uma saída PWM.

## Como funciona

- **Geração do sinal em Python**: [main.py](main.py) gera um vetor senoidal (200 Hz, 100 amostras) já convertido para valores de largura de pulso (`duty = load * x/2`), consistentes com o `load` do PWM configurado no firmware.
- **Recepção via UART + µDMA**: o vetor é enviado como `float32` pela UART0 (115200 bps) e recebido no Tiva pelo canal `UDMA_CHANNEL_UART0RX`, sem intervenção da CPU durante a transferência.
- **Atualização síncrona pelo Timer**: a interrupção periódica do Timer0, na frequência de amostragem (10 kHz), avança um índice circular sobre o buffer recebido e atualiza a largura de pulso do PWM (`PWMPulseWidthSet`) a cada ciclo — reconstruindo o sinal na saída PWM (20 kHz, pino `PF2`).
- **Recuperação do sinal**: um filtro passa-baixas analógico na saída do PWM remove as componentes de chaveamento, restando o sinal analógico reconstruído.

## Arquivos

| Arquivo | Descrição |
|---|---|
| [main.c](main.c) | Firmware do Tiva: configuração de PWM, Timer, UART e uDMA, e atualização da razão cíclica na interrupção do timer. |
| [main.py](main.py) | Geração do sinal senoidal e envio do vetor de duty cycles pela serial. |
| [startup_ccs.c](startup_ccs.c) | Vetor de interrupções e inicialização de runtime para o Code Composer Studio. |

## Como executar

1. Compile e grave [main.c](main.c) no Tiva via Code Composer Studio.
2. Conecte um filtro passa-baixas na saída PWM (`PF2`) para recuperar o sinal analógico.
3. Ajuste a porta serial em [main.py](main.py) e execute o script para enviar o vetor de referência.
4. Observe a saída filtrada no osciloscópio.

## Parâmetros principais

- Frequência de PWM: 20 kHz (clock do PWM = clock do sistema / 64)
- Frequência de amostragem (atualização do duty): 10 kHz
- Clock do sistema: 120 MHz
- Baud rate UART: 115200 bps

## Observações de projeto

Há um compromisso entre **resolução do conversor** (definida pelo `load` do PWM, i.e. número de níveis de duty possíveis) e **frequência de PWM**: aumentar a frequência de chaveamento facilita a filtragem das harmônicas, mas reduz o `load` disponível (para um clock de PWM fixo) e, portanto, a resolução efetiva do DAC.
