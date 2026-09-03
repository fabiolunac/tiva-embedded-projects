#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/fpu.h"
#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "driverlib/adc.h"
#include "utils/uart_utils.h"
#include "arm_math.h" // Biblioteca CMSIS-DSP
#include <math.h>     // Para sqrtf e funções trigonométricas

#define NUM_SEC 0.5
#define PWM_FREQUENCY 4000     //Frequ�ncia do PWM
#define PWM_PERIOD 5e-5         //Per�odo do PWM

//Coeficientes do sensor
#define COEFF_A 0.0217529296875       //Ganho do sensor
#define COEFF_B 1861                  //Offset do sensor

//Parâmetros do controlador
#define B0 0.026025f
#define B1 0.005270f
#define B2 -0.020755f
#define A1 -0.777969f
#define A2 -0.222031f

uint32_t g_ui32SysClock;

volatile uint16_t pwm_load=0;
int16_t duty_a = 0, duty_b = 0, duty_c = 0;

uint32_t adc_id = 0, adc_iq = 0, adc_rho = 0;

//Variaveis para controle direto
float yd[3] = {0};
float xd[3] = {0};

//Variaveis para controle de quadratura
float yq[3] = {0};
float xq[3] = {0};

void park_to_abc(float id, float iq, float theta, float *ia, float *ib, float *ic) {
    float cos_theta = arm_cos_f32(theta);
    float sin_theta = arm_sin_f32(theta);
    float alpha = id * cos_theta - iq * sin_theta;
    float beta = id * sin_theta + iq * cos_theta;

    *ia = alpha;
    *ib = (-0.5f * alpha) + ((sqrtf(3.0f) / 2.0f) * beta);
    *ic = (-0.5f * alpha) - ((sqrtf(3.0f) / 2.0f) * beta);
}

void PWM0Gen1IntHandler(void)
{

    PWMGenIntClear(PWM0_BASE, PWM_GEN_1, PWM_INT_CNT_LOAD);

    while(!ADCIntStatus(ADC0_BASE, 3, false));
    uint32_t adcValues[2];
    ADCIntClear(ADC0_BASE, 3);
    ADCSequenceDataGet(ADC0_BASE, 3, adcValues);
    adc_id = adcValues[0];
    adc_rho = adcValues[1];

    while(!ADCIntStatus(ADC1_BASE, 3, false));
    ADCIntClear(ADC1_BASE, 3);
    ADCSequenceDataGet(ADC1_BASE, 3, &adc_iq);


    float Id = adc_id * COEFF_A - COEFF_B;
    float Iq = adc_iq * COEFF_A - COEFF_B;
    float rho = adc_rho * COEFF_A - COEFF_B;

    // Controle Direto
    xd[2] = xd[1];
    xd[1] = xd[0];
    xd[0] = 1800 - Id;
    yd[2] = yd[1];
    yd[1] = yd[0];
    yd[0] = B0 * xd[0] + B1 * xd[1] + B2 * xd[2] - A1 * yd[1] - A2 * yd[2];

    // Controle de quadratura
    xq[2] = xq[1];
    xq[1] = xq[0];
    xq[0] = 1800 - Iq;
    yq[2] = yq[1];
    yq[1] = yq[0];
    yq[0] = B0 * xq[0] + B1 * xq[1] + B2 * xq[2] - A1 * yq[1] - A2 * yq[2];


    float ia, ib, ic;

    park_to_abc(yd[0], yq[0], rho, &ia, &ib, &ic);

    duty_a = (pwm_load * ia) / 100;
    duty_b = (pwm_load * ib) / 100;
    duty_c = (pwm_load * ic) / 100;

    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_0, duty_a);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, duty_b);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, duty_c);


    PWMSyncUpdate(PWM0_BASE, PWM_GEN_0_BIT | PWM_GEN_1_BIT);
}

void configUART(uint32_t sys_clk_freq)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(
            UART0_BASE, sys_clk_freq, 115200,
            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
}

void config_adc()
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

    ADCReferenceSet(ADC0_BASE, ADC_REF_INT);
    IntDisable(INT_ADC0SS0);
    ADCIntDisable(ADC0_BASE, 3);
    ADCSequenceDisable(ADC0_BASE, 3);

    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PWM1, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH2);
    ADCSequenceStepConfigure(ADC0_BASE, 3, 1, ADC_CTL_CH4 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 3);
    ADCIntClear(ADC0_BASE, 3);

    ADCReferenceSet(ADC1_BASE, ADC_REF_INT);
    IntDisable(INT_ADC1SS0);
    ADCIntDisable(ADC1_BASE, 3);
    ADCSequenceDisable(ADC1_BASE, 3);
    ADCSequenceConfigure(ADC1_BASE, 3, ADC_TRIGGER_PWM1, 0);
    ADCSequenceStepConfigure(ADC1_BASE, 3, 0, ADC_CTL_CH3 | ADC_CTL_END | ADC_CTL_IE);
    ADCSequenceEnable(ADC1_BASE, 3);
    ADCIntClear(ADC1_BASE, 3);


}


void config_pwm(uint32_t sys_clock)
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    GPIOPinConfigure(GPIO_PF1_M0PWM1);
    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPinConfigure(GPIO_PF2_M0PWM2);


    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_3);

    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_1);

    PWMGenConfigure(PWM0_BASE, PWM_GEN_0,
                        PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_SYNC);
    PWMGenConfigure(PWM0_BASE, PWM_GEN_1,
                           PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_SYNC);
    PWMGenConfigure(PWM0_BASE, PWM_GEN_2,
                               PWM_GEN_MODE_UP_DOWN | PWM_GEN_MODE_SYNC);


    PWMSyncTimeBase(PWM0_BASE, PWM_GEN_0_BIT | PWM_GEN_1_BIT);


    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, (sys_clock/ PWM_FREQUENCY));
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, (sys_clock/ PWM_FREQUENCY));
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_2, (sys_clock/ PWM_FREQUENCY));

    PWMGenIntTrigEnable(PWM0_BASE, PWM_GEN_1, PWM_INT_CNT_ZERO | PWM_TR_CNT_LOAD);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1,0);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2,0);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_3,0);

    PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT, true);
    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
    PWMOutputState(PWM0_BASE, PWM_OUT_3_BIT, true);

    PWMOutputInvert(PWM0_BASE, PWM_OUT_2_BIT, true);

    PWMGenEnable(PWM0_BASE, PWM_GEN_0);
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
    PWMGenEnable(PWM0_BASE, PWM_GEN_2);

    PWMSyncUpdate(PWM0_BASE, PWM_GEN_0_BIT | PWM_GEN_1_BIT | PWM_GEN_2_BIT);

    pwm_load = PWMGenPeriodGet(PWM0_BASE, PWM_GEN_1);
}


int
main(void)
{

    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_240), 120000000);


    FPUEnable();
    FPUStackingEnable();

    config_adc();
    config_pwm(g_ui32SysClock);
    configUART(g_ui32SysClock);
    IntMasterEnable();

    PWMIntEnable(PWM0_BASE, PWM_INT_GEN_1);
    PWMGenIntTrigEnable(PWM0_BASE, PWM_GEN_1, PWM_INT_CNT_LOAD);
    IntEnable(INT_PWM0_1);


    while(1)
    {

    }
}
