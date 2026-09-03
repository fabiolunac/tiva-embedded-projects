#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "inc/tm4c1294ncpdt.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/adc.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "driverlib/fpu.h"
#include "inc/hw_uart.h"
#include "driverlib/pwm.h"
#include "arm_math.h"

#define SIGNAL_SIZE 500
#define BLOCK_SIZE 25
#define SAMPLE_FREQUENCY 10000
#define NUM_TAPS 41
#define NUM_TAPS_ARRAY_SIZE 41
#define ADC_SEQUENCER 1
#define PWM_FREQUENCY 20000

uint32_t ui32SysClkFreq;
uint32_t adcValue;
uint16_t index = 0;
uint16_t index_pwm = 0;
uint32_t ui32Load;
uint32_t ui32PWMClock;
uint32_t flag = 0;

float signal_in [SIGNAL_SIZE];
float signal_out[SIGNAL_SIZE];
float valor_normalizado;

float32_t signal_out_arm[SIGNAL_SIZE];

static float32_t firStateF32[BLOCK_SIZE + NUM_TAPS - 1];

float firCoeffs32[NUM_TAPS_ARRAY_SIZE] = {
   -0.0000661482f, +0.000122248f, +0.000377046f, +0.000875547f, +0.001712373f, +0.002996174f, +0.004836332f, +0.007329607f, +0.01054662f, +0.014518033f, +0.019222752f, +0.024579685f,
    +0.030444721f, +0.036613534f, +0.042831135f, +0.048807534f, +0.054238442f, +0.058828748f, +0.062316905f, +0.064497453f, +0.065239319f, +0.064497453f, +0.062316905f, +0.058828748f,
    +0.054238442f, +0.048807534f, +0.042831135f, +0.036613534f, +0.030444721f, +0.024579685f, +0.019222752f, +0.014518033f, +0.01054662f,  +0.007329607f, +0.004836332f, +0.002996174f,
    +0.001712373f, +0.000875547f, +0.000377046f, +0.000122248f, -0.0000661482f
};

uint32_t numBlocks = SIGNAL_SIZE / BLOCK_SIZE;

void filter_ARM(float* signal_input, float32_t* signal_output, float32_t* coeffs, float32_t* firState ,uint32_t block_size, uint32_t num_taps, uint32_t num_blocks)
{
    static arm_fir_instance_f32 S;
    arm_fir_init_f32(&S, num_taps, (float32_t*) coeffs, firState, block_size);

    uint32_t i;
    for (i = 0; i < num_blocks; i++)
    {
        arm_fir_f32(&S, signal_input + (i * block_size), signal_output + (i * block_size),
                    block_size);
    }
}


void generate_signal(float *signal, uint16_t signal_size, float sample_frequency)
{
    uint16_t i = 0;
    uint16_t f1 = 100;
    uint16_t f2 = 1000;

    for (i = 0; i < signal_size; i++)
    {
        float t = i/sample_frequency;
        float sine_value = sinf(2 * PI * f1 * t) + sinf(2 * PI * f2 * t);
        signal[i] = sine_value;
    }
}


void filter_FIR(float* signal_input, float* signal_output, float32_t* coeffs, uint32_t signal_size, uint32_t num_taps)
{
    uint32_t n;
    uint32_t j;

    for (n = 0; n < signal_size; n++)
    {
       float sum = 0;
       for (j = 0; j < num_taps; j++)
       {
           if (n >= j)
           {
               sum += coeffs[j] * signal_input[n - j];
           }

       }
       signal_output[n] = sum;
    }
}

void configureADC(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0));

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));

    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

    ADCSequenceConfigure(ADC0_BASE, ADC_SEQUENCER, ADC_TRIGGER_TIMER, 0);

    ADCSequenceStepConfigure(ADC0_BASE, ADC_SEQUENCER, 3, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceEnable(ADC0_BASE, ADC_SEQUENCER);

    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);
}

void configureTimer(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32SysClkFreq/SAMPLE_FREQUENCY - 1);

    TimerControlTrigger(TIMER0_BASE, TIMER_A, true);

    IntEnable(INT_TIMER0A);

    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    TimerEnable(TIMER0_BASE, TIMER_A);
}

void configurePWM(void)
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_1);

    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);

    ui32PWMClock = ui32SysClkFreq;

    ui32Load = (ui32PWMClock/PWM_FREQUENCY) - 1;

    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN);

    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, ui32Load);

    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, ui32Load/2);

    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);

    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
}


void Timer0IntHandler(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    valor_normalizado = (float)signal_out[index_pwm]/2;

    uint32_t duty = 5999 * (float)valor_normalizado;

    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, duty);

    index_pwm = (index_pwm + 1) % SIGNAL_SIZE;

}


int main(void)
{

    ui32SysClkFreq = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    FPULazyStackingEnable();

    FPUEnable();

    IntMasterEnable();

    configureADC();

    configureTimer();

    configurePWM();

    while(1)
    {
        if (ADCIntStatus(ADC0_BASE, ADC_SEQUENCER, false))
        {
            ADCIntClear(ADC0_BASE, ADC_SEQUENCER);

            while(!ADCIntStatus(ADC0_BASE, ADC_SEQUENCER, false)){}

            ADCSequenceDataGet(ADC0_BASE, ADC_SEQUENCER, &adcValue);

            signal_in[index++] = adcValue*(3.3f/4095.0f);

            if(index == (SIGNAL_SIZE-1))
            {
                filter_FIR(signal_in, signal_out, firCoeffs32, SIGNAL_SIZE, NUM_TAPS_ARRAY_SIZE);

                filter_ARM(signal_in, signal_out_arm, firCoeffs32, firStateF32, BLOCK_SIZE, NUM_TAPS, numBlocks);

                index = 0;

            }
        }
    }



}
