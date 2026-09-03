# Tiva Embedded Projects

Projetos de sistemas embarcados desenvolvidos no microcontrolador **Texas Instruments Tiva C Series TM4C1294** (ARM Cortex-M4F), na disciplina de Microcontroladores e DSP do Programa de Pós-Graduação em Engenharia Elétrica da UFJF.

Todo o firmware foi desenvolvido em **C**, no **Code Composer Studio**, utilizando a **TivaWare Peripheral Driver Library** e a **CMSIS-DSP**. As rotinas de apoio (geração de sinais, análise e projeto de filtros e controladores) foram escritas em **Python**.

O conjunto cobre a cadeia completa de um sistema de aquisição e controle em tempo real: condicionamento analógico, amostragem, processamento digital de sinais, conversão DA e controle digital de conversores de potência, com validação em PIL e HIL.

---

## Visão geral

| Projeto | Tema | Periféricos e recursos |
|---|---|---|
| [1 - Data Acquisition](1-data-acquisition/) | Módulo de aquisição de dados com filtro anti-aliasing | ADC, Timer, UART, µDMA |
| [2 - PWM DAC](2-pwm-dac/) | Conversor digital-analógico por PWM | PWM, Timer, UART, µDMA |
| [3 - FIR Filter](3-fir-filter/) | Filtro FIR embarcado, manual vs. CMSIS-DSP | ADC, Timer, PWM, FPU, CMSIS-DSP |
| [4 - Inverter Control](4-inverter-control/) | Controle de corrente de inversor trifásico (PIL e HIL) | ADC, PWM, interrupções, UART, FPU, CMSIS-DSP |

---

## 1 - Data Acquisition

Módulo de aquisição de dados com o Tiva atuando como servidor de amostragem para uma aplicação em Python.

- Filtro anti-aliasing analógico Sallen-Key de 4ª ordem (TL082), com frequência de corte de 500 Hz projetada uma década abaixo da frequência de Nyquist.
- Amostragem a 10 kHz pelo ADC de 12 bits, disparada por **trigger de timer** em vez de polling, garantindo período determinístico.
- Transferência dos buffers pela UART com **µDMA**, liberando a CPU durante a transmissão.
- Análise em Python (FFT) demonstrando o fenômeno de *aliasing* e o efeito do filtro sobre componentes acima de Nyquist.

## 2 - PWM DAC

Implementação de um conversor digital-analógico por modulação PWM, já que o TM4C1294 não possui DAC integrado.

- Sinal senoidal gerado em Python e transmitido ao microcontrolador pela UART com µDMA.
- Atualização da razão cíclica na interrupção periódica do timer, na frequência de amostragem.
- Filtro passa-baixas na saída para recuperação do sinal analógico.
- Análise do compromisso de projeto entre **resolução do conversor** e **frequência de PWM**, e do impacto da banda do filtro sobre as harmônicas de chaveamento (ensaios de 2 kHz a 100 kHz).

## 3 - FIR Filter

Filtragem digital embarcada em tempo real, com comparação de desempenho computacional entre implementações.

- Filtro FIR passa-baixas de 40 coeficientes (corte em ~800 Hz, fs = 10 kHz), projetado com `pyfda`, para separar as componentes de 100 Hz e 1 kHz de um sinal de entrada.
- Duas implementações: convolução escrita manualmente e função `arm_fir_f32` da **CMSIS-DSP**, com FPU habilitada.
- **Benchmark por contagem de ciclos de clock** entre breakpoints:

  | Implementação | Ciclos | Tempo total | Tempo por amostra | Ordem máx. teórica |
  |---|---|---|---|---|
  | Manual | 306 512 | 2,55 ms | 62,2 µs | 66 |
  | CMSIS-DSP | 75 241 | 0,63 ms | 15,4 µs | 266 |

- A partir do orçamento de tempo (Ts = 100 µs), estimativa da ordem máxima de filtro viável em cada caso, evidenciando o ganho de ~4x da biblioteca otimizada.

## 4 - Inverter Control

Controle digital de corrente de um inversor trifásico com carga RL conectada à rede, em coordenadas síncronas.

**Projeto do controlador**
- Planta RL modelada em coordenadas dq e reduzida a primeira ordem por realimentação e desacoplamento de distúrbios.
- Compensador de avanço projetado no domínio da frequência para margem de fase de 60° e frequência de cruzamento de 600 Hz; parâmetros obtidos numericamente com `scipy.optimize.fsolve`.
- Discretização pelo método bilinear (Tustin) a 4 kHz, implementada no Tiva como equação de diferenças de segunda ordem.

**PIL — Processor in the Loop** ([`pil/`](4-inverter-control/pil/))
- Planta simulada no **PLECS**, trocando dados com o Tiva pela UART através de um bloco C-script.
- Controle executado no microcontrolador sobre os eixos direto e de quadratura, com transformada inversa de Park e Clarke usando `arm_sin_f32` e `arm_cos_f32`.
- Validação por degrau de referência de corrente, com o controle mantendo o novo valor após o transitório.

**HIL — Hardware in the Loop** ([`hil/`](4-inverter-control/hil/))
- Planta emulada em tempo real no **Typhoon HIL**, com sinais físicos: leitura de Id, Iq e do ângulo da PLL por dois módulos ADC e geração dos três PWMs de chaveamento.
- Amostragem do ADC **disparada pelo próprio PWM** (`ADC_TRIGGER_PWM1`), sincronizando aquisição e chaveamento.
- Malha de controle completa executada dentro da **rotina de interrupção do PWM**, garantindo período determinístico, com atualização sincronizada das três larguras de pulso (`PWMSyncUpdate`).
- Geradores PWM configurados em modo up/down sincronizados, clock de sistema em 120 MHz e FPU habilitada.

---

## Ambiente

- **Hardware:** Tiva C Series EK-TM4C1294XL, NI ELVIS II+ (gerador de funções e osciloscópio), Typhoon HIL
- **Firmware:** C, Code Composer Studio, TivaWare, CMSIS-DSP
- **Simulação e análise:** PLECS, Python (NumPy, SciPy, Matplotlib, pySerial, pyfda)

## Autor

**Fábio Cardani Luna** — Engenheiro Eletricista, mestre e doutorando em Engenharia Elétrica pela UFJF.
