# PIL — Processor in the Loop

Validação do controlador de corrente do inversor com a planta simulada no **PLECS**, trocando dados com o Tiva pela UART através de um bloco C-script — o microcontrolador executa o controle real, em tempo real de simulação, sobre uma planta puramente simulada.

## Como funciona

- **Troca de dados via UART**: a cada passo de simulação, o PLECS envia pela UART0 (115200 bps) 13 floats — tensões e correntes de fase (`va, vb, vc, ia, ib, ic`), referências (`Id_ref, Iq_ref`), ângulo da PLL (`rho`) e realimentações de desacoplamento (`Vd, Vq, Id, Iq`) — e aguarda de volta as três referências de tensão de fase.
- **Transformada de Clarke/Park (CMSIS-DSP)**: `clarke_park_transform` converte as correntes de fase para o referencial síncrono dq usando `arm_clarke_f32`, `arm_park_f32`, `arm_sin_f32` e `arm_cos_f32`.
- **Controle**: dois controladores de 2ª ordem independentes (equação de diferenças, coeficientes `B0, B1, B2, A1, A2`) atuam sobre o erro de corrente nos eixos `d` e `q`.
- **Transformada inversa**: `park_to_abc` reconstrói as três tensões de fase de saída (`iai, ibi, ici`) a partir das saídas dos controladores e do ângulo `rho`, normalizadas pelo barramento CC (`VCC`).
- **Validação**: aplicação de um degrau na referência de corrente (`Id_ref`/`Iq_ref`) no PLECS, verificando que o controle converge e mantém o novo valor de referência após o transitório.

## Arquivos

| Arquivo | Descrição |
|---|---|
| [main.c](main.c) | Firmware do Tiva: recepção/envio via UART, transformadas de Clarke/Park (CMSIS-DSP) e malha de controle dq. |
| [startup_ccs.c](startup_ccs.c) | Vetor de interrupções e inicialização de runtime para o Code Composer Studio. |

## Como executar

1. Compile e grave [main.c](main.c) no Tiva via Code Composer Studio (com a CMSIS-DSP adicionada ao projeto).
2. No PLECS, monte a planta RL trifásica conectada à rede e um bloco C-script configurado para comunicar pela mesma porta serial usada pelo Tiva (115200 bps), enviando/recebendo os floats na ordem esperada por `main.c`.
3. Rode a simulação e aplique um degrau na referência de corrente para validar a resposta do controlador.

## Parâmetros principais

- Clock do sistema: 120 MHz, FPU habilitada
- Baud rate UART: 115200 bps
- Controlador discretizado a 4 kHz (Tustin), coeficientes `B0 = 0,026025`, `B1 = 0,005270`, `B2 = -0,020755`, `A1 = -0,777969`, `A2 = -0,222031`
