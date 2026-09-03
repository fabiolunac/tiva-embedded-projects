# 3 - FIR Filter

Filtragem digital FIR embarcada em tempo real, comparando uma implementação manual (convolução) com a função otimizada `arm_fir_f32` da **CMSIS-DSP**, em termos de desempenho computacional.

## Como funciona

- **Sinal de teste**: gerado internamente no Tiva (`generate_signal`) como a soma de duas senoides, 100 Hz e 1 kHz, amostradas a 10 kHz.
- **Filtro**: FIR passa-baixas de 41 coeficientes (corte em ~800 Hz), projetado no `pyfda`, embarcado como vetor `firCoeffs32[]`.
- **Duas implementações do mesmo filtro**:
  - `filter_FIR`: convolução direta escrita manualmente, amostra a amostra.
  - `filter_ARM`: `arm_fir_f32` da CMSIS-DSP, processando o sinal em blocos (`BLOCK_SIZE = 25`), com FPU habilitada (`FPUEnable`/`FPULazyStackingEnable`).
- **Saída em PWM**: o resultado filtrado é reproduzido em uma saída PWM (20 kHz, pino `PF2`) atualizada pela interrupção do Timer0, para recuperação analógica com filtro externo — mesmo princípio do [PWM DAC](../2-pwm-dac/).
- **Benchmark**: as duas implementações são cronometradas por contagem de ciclos de clock entre breakpoints no depurador do Code Composer Studio.

| Implementação | Ciclos | Tempo total | Tempo por amostra | Ordem máx. teórica* |
|---|---|---|---|---|
| Manual | 306 512 | 2,55 ms | 62,2 µs | 66 |
| CMSIS-DSP | 75 241 | 0,63 ms | 15,4 µs | 266 |

\* Ordem máxima de filtro viável dentro do orçamento de tempo por amostra (Ts = 100 µs, fs = 10 kHz).

## Arquivos

| Arquivo | Descrição |
|---|---|
| [main.c](main.c) | Firmware do Tiva: geração do sinal, ADC, as duas implementações do filtro FIR e saída em PWM. |
| [startup_ccs.c](startup_ccs.c) | Vetor de interrupções e inicialização de runtime para o Code Composer Studio. |

## Como executar

1. Compile e grave [main.c](main.c) no Tiva via Code Composer Studio (com a CMSIS-DSP adicionada ao projeto).
2. Para o benchmark, insira breakpoints antes/depois das chamadas de `filter_FIR` e `filter_ARM` em `main()` e leia a contagem de ciclos no depurador.
3. Para observar a saída filtrada, conecte um osciloscópio ao pino `PF2` (PWM) através de um filtro passa-baixas.

## Parâmetros principais

- Frequência de amostragem: 10 kHz
- Frequência de PWM: 20 kHz
- Clock do sistema: 120 MHz, FPU habilitada
