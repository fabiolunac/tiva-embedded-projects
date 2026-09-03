# 4 - Inverter Control

Controle digital de corrente de um inversor trifásico com carga RL conectada à rede, em coordenadas síncronas (dq), validado em duas configurações de bancada: **PIL** (Processor in the Loop) e **HIL** (Hardware in the Loop).

## Projeto do controlador

- Planta RL modelada em coordenadas dq e reduzida a primeira ordem por realimentação e desacoplamento de distúrbios (feedforward de tensão e do termo de acoplamento cruzado ωL).
- Compensador de avanço de fase projetado no domínio da frequência, para margem de fase de 60° e frequência de cruzamento de 600 Hz; parâmetros do compensador obtidos numericamente com `scipy.optimize.fsolve`.
- Discretização pelo método bilinear (Tustin) a 4 kHz, implementada no Tiva como equação de diferenças de 2ª ordem (coeficientes `B0, B1, B2, A1, A2`), um controlador independente para cada eixo (direto `d` e de quadratura `q`).
- As duas versões compartilham a mesma lei de controle (`park_to_abc`, coeficientes do controlador); o que muda entre elas é **de onde vêm as medidas** e **como a malha é fechada**.

## Subprojetos

| Configuração | Planta | Malha de controle |
|---|---|---|
| [`pil/`](pil/) — Processor in the Loop | Simulada no **PLECS** | Fechada pela UART, via bloco C-script do PLECS |
| [`hil/`](hil/) — Hardware in the Loop | Emulada em tempo real no **Typhoon HIL** | Fechada por ADC/PWM físicos, sincronizada pelo próprio PWM |

## Ambiente

- **Hardware:** Tiva C Series EK-TM4C1294XL, Typhoon HIL
- **Firmware:** C, Code Composer Studio, TivaWare, CMSIS-DSP (transformadas de Park/Clarke, `arm_sin_f32`/`arm_cos_f32`)
- **Simulação:** PLECS (PIL), Python/SciPy para projeto do compensador
