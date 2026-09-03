#include <stdint.h>
#include <stdbool.h>
#include <string.h>
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
#include "inc/hw_uart.h"
#include "driverlib/pwm.h"

#define SIGNAL_SIZE 100
#define PWM_FREQUENCY 20000
#define SAMPLE_FREQUENCY 10000

uint32_t ui32SysClkFreq;
uint32_t ui32Load;
uint32_t ui32PWMClock;
uint32_t flag_transfer = 0;
uint32_t index = 0;
float signal[SIGNAL_SIZE];


// Configuração da Tabela de controle do uDMA
#if defined(ewarm)
#pragma data_alignment=1024
uint8_t pui8ControlTable[1024];
#elif defined(ccs)
#pragma DATA_ALIGN(pui8ControlTable, 1024)
uint8_t pui8ControlTable[1024];
#else
uint8_t pui8ControlTable[1024] __attribute__ ((aligned(1024)));
#endif

void ConfigureTimer(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32SysClkFreq/SAMPLE_FREQUENCY - 1);

    IntEnable(INT_TIMER0A);

    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    TimerEnable(TIMER0_BASE, TIMER_A);
}

void ConfigurePWM(void)
{

    // Habilitar o PWM e os módulos de GPIO
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    // PWM do clock
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_64);

    // Configuração do GPIO PG0 para PWM
    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);

    // Clock do PWM e valor do LOAD
    ui32PWMClock = ui32SysClkFreq / 64; // 120MHz/64

    ui32Load = (ui32PWMClock/PWM_FREQUENCY) - 1; // 1875000/100000

    PWMGenConfigure(PWM0_BASE, PWM_GEN_1, PWM_GEN_MODE_DOWN);

    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1, ui32Load);

    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, ui32Load/2);

    PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);

    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
}


// ---------------------------------------------
// UART
// ---------------------------------------------

void ConfigureUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    UARTConfigSetExpClk(UART0_BASE, ui32SysClkFreq, 115200,(UART_CONFIG_WLEN_8 |
            UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    UARTFIFOLevelSet(UART0_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);

    UARTEnable(UART0_BASE);
}

// ---------------------------------------------
// uDMA
// ---------------------------------------------

void ConfigureUDMA(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UDMA)){}

    uDMAEnable();
    uDMAControlBaseSet(pui8ControlTable);

    UARTDMAEnable(UART0_BASE, UART_DMA_TX | UART_DMA_RX);

    uDMAChannelAssign(UDMA_CH8_UART0RX);

    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART0RX, UDMA_ATTR_ALL);
    uDMAChannelControlSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_4);
}

// ---------------------------------------------
// Receber o vetor via uDMA
// ---------------------------------------------


void Receive_Vector(void)
{


    uDMAChannelTransferSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT,
                           UDMA_MODE_BASIC,
                           (void *)(UART0_BASE + UART_O_DR),
                           signal,
                           SIGNAL_SIZE * sizeof(float));

    uDMAChannelEnable(UDMA_CHANNEL_UART0RX);

    while(uDMAChannelIsEnabled(UDMA_CHANNEL_UART0RX));

}

void Timer0IntHandler(void)
{

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    uint32_t duty = signal[index];

    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2, duty);

    index = (index + 1) % SIGNAL_SIZE;
}



// ---------------------------------------------
// Main
// ---------------------------------------------
int main(void)
{

    ui32SysClkFreq = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

    ConfigurePWM();

    ConfigureTimer();

    ConfigureUART();

    ConfigureUDMA();

    IntMasterEnable();

    while(1)
    {
        Receive_Vector();
    }
}
