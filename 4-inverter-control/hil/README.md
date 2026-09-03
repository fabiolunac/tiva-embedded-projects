# HIL — Hardware in the Loop

Validação do controlador de corrente do inversor com a planta emulada em tempo real no **Typhoon HIL**, fechando a malha por sinais físicos: leitura analógica de Id, Iq e do ângulo da PLL, e geração dos três PWMs de chaveamento.

## Como funciona

- **Amostragem disparada pelo PWM**: os dois módulos ADC (`ADC0`, `ADC1`) são configurados com `ADC_TRIGGER_PWM1`, sincronizando a aquisição de corrente/ângulo com o próprio chaveamento, em vez de um timer independente. `ADC0` lê `Id` e `rho`; `ADC1` lê `Iq`.
- **Malha de controle na interrupção do PWM**: toda a malha — leitura do ADC, controle dq e atualização das três larguras de pulso — roda dentro de `PWM0Gen1IntHandler`, garantindo período determinístico a cada ciclo de PWM (4 kHz).
- **Controle dq**: mesma lei de controle do [PIL](../pil/) (equação de diferenças de 2ª ordem por eixo), com referência fixa de 1800 (contagens do ADC, já no referencial do sensor) para `Id` e `Iq`.
- **Transformada inversa de Park/Clarke**: `park_to_abc` converte as saídas dos controladores para as três referências de fase, usando `arm_sin_f32`/`arm_cos_f32` da CMSIS-DSP.
- **Atualização síncrona do PWM**: os três geradores PWM (`PWM_GEN_0/1/2`) operam em modo up/down sincronizado (`PWM_GEN_MODE_SYNC`), com as larguras de pulso atualizadas atomicamente por `PWMSyncUpdate` ao final de cada interrupção.

## Arquivos

| Arquivo | Descrição |
|---|---|
| [main.c](main.c) | Firmware do Tiva: configuração de ADC (disparado por PWM), PWM sincronizado trifásico, malha de controle dq na interrupção. |
| [startup_ccs.c](startup_ccs.c) | Vetor de interrupções e inicialização de runtime para o Code Composer Studio. |

## Como executar

1. Compile e grave [main.c](main.c) no Tiva via Code Composer Studio (com a CMSIS-DSP adicionada ao projeto).
2. No Typhoon HIL, monte a planta RL trifásica conectada à rede e faça a interface física com o Tiva: saídas analógicas do HIL para os canais ADC (`PE1`, `PE2`, `PE3` — Iq, Id, rho) e entradas PWM do HIL a partir de `PF1`/`PF2`/`PF3`.
3. Rode a emulação em tempo real e observe a resposta da malha de corrente.

## Parâmetros principais

- Clock do sistema: 120 MHz, FPU habilitada (`FPUStackingEnable`)
- Frequência de PWM / controle: 4 kHz (`PWM_FREQUENCY`), geradores em modo up/down sincronizados
- Controlador: mesmos coeficientes discretizados por Tustin do [PIL](../pil/) (`B0, B1, B2, A1, A2`)
- Coeficientes do sensor de corrente: `COEFF_A = 0,0217529296875`, `COEFF_B = 1861`
