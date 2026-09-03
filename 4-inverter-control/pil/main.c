#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uart_utils.h"
#include "driverlib/fpu.h"
#include "arm_math.h" // Biblioteca CMSIS-DSP
#include <math.h>     // Para sqrtf e funções trigonométricas

// Coeficientes do controlador
#define B0 0.026025f
#define B1 0.005270f
#define B2 -0.020755f
#define A1 -0.777969f
#define A2 -0.222031f


// Parametros
#define VCC 1000


// Variáveis globais
float ia, ib, ic, Id_ref, rho, Vd, Iq_ref, Vq, va, vb, vc, Id, Iq;
float id, iq, iai, ibi, ici, vd, vq;
float alpha, beta;

// Variáveis de controle
float xd[3]={0};
float yd[3]={0};
float xq[3]={0};
float yq[3]={0};

uint32_t ui32SysClkFreq;



// Transformada Clarke e Park
void clarke_park_transform(float ia, float ib, float theta, float *id, float *iq) {
    float alpha, beta;

    // Transformada de Clarke usando CMSIS
    arm_clarke_f32(ia, ib, &alpha, &beta);

    // Cálculo de Park usando CMSIS com theta
    float cos_theta = arm_cos_f32(theta);
    float sin_theta = arm_sin_f32(theta);
    arm_park_f32(alpha, beta, id, iq, cos_theta, sin_theta);
}



// Transformada inversa de Park e Clarke
void park_to_abc(float id, float iq, float theta, float *ia, float *ib, float *ic) {
    float cos_theta = arm_cos_f32(theta);
    float sin_theta = arm_sin_f32(theta);
    alpha = id * cos_theta - iq * sin_theta;
    beta = id * sin_theta + iq * cos_theta;

    *ia = alpha;
    *ib = (-0.5f * alpha) + ((sqrtf(3.0f) / 2.0f) * beta);
    *ic = (-0.5f * alpha) - ((sqrtf(3.0f) / 2.0f) * beta);
}


int main(void) {
    // Configuração do clock
    ui32SysClkFreq = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                        SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    FPULazyStackingEnable();

    FPUEnable();

    // Inicialização da UART
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, ui32SysClkFreq, 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Loop principal
    while (1) {
        if (UARTCharsAvail(UART0_BASE)) {
            recv_float(UART0_BASE, &va);
            recv_float(UART0_BASE, &vb);
            recv_float(UART0_BASE, &vc);
            recv_float(UART0_BASE, &ia);
            recv_float(UART0_BASE, &ib);
            recv_float(UART0_BASE, &ic);
            recv_float(UART0_BASE, &Id_ref);
            recv_float(UART0_BASE, &rho);
            recv_float(UART0_BASE, &Vd);
            recv_float(UART0_BASE, &Iq_ref);
            recv_float(UART0_BASE, &Vq);
            recv_float(UART0_BASE, &Id);
            recv_float(UART0_BASE, &Iq);

            // Controle Direto
            xd[2] = xd[1];
            xd[1] = xd[0];
            xd[0] = Id_ref - Id;
            yd[2] = yd[1];
            yd[1] = yd[0];
            yd[0] = B0 * xd[0] + B1 * xd[1] + B2 * xd[2] - A1 * yd[1] - A2 * yd[2];

            // Controle de quadratura
            xq[2] = xq[1];
            xq[1] = xq[0];
            xq[0] = Iq_ref - Iq;
            yq[2] = yq[1];
            yq[1] = yq[0];
            yq[0] = B0 * xq[0] + B1 * xq[1] + B2 * xq[2] - A1 * yq[1] - A2 * yq[2];

            // Transformada inversa
             park_to_abc(yd[0], yq[0], rho, &iai, &ibi, &ici);
             iai = iai*2/VCC;
             ibi = ibi*2/VCC;
             ici = ici*2/VCC;

            // Envio das saídas
            send_float(UART0_BASE, iai);
            send_float(UART0_BASE, ibi);
            send_float(UART0_BASE, ici);

        }
    }
}
