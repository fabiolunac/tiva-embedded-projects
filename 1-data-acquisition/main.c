#include <stdint.h>
#include <stdbool.h>
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

#define ADC_SEQUENCER 1
#define SIGNAL_SIZE 50*4 // (Fs/f)*Nc

uint32_t ui32SysClkFreq;
uint32_t adcValue;
uint32_t flag_transfer = 0;
uint32_t SampleFrequency = 10000;
uint32_t signal[SIGNAL_SIZE];
uint32_t sampleIndex = 0;


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




// ---------------------------------------------
// ADC
// ---------------------------------------------

void ConfigureADC(void)
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

// ---------------------------------------------
// TIMER
// ---------------------------------------------

void ConfigureTimer(void)
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32SysClkFreq/SampleFrequency - 1);

    TimerControlTrigger(TIMER0_BASE, TIMER_A, true);

    TimerEnable(TIMER0_BASE, TIMER_A);
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

    uDMAChannelAssign(UDMA_CH9_UART0TX);
    uDMAChannelAssign(UDMA_CH8_UART0RX);

    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART0TX, UDMA_ATTR_ALL);
    uDMAChannelControlSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_4);

    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART0RX, UDMA_ATTR_ALL);
    uDMAChannelControlSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_4);
}

// ---------------------------------------------
// Envio do vetor via uDMA
// ---------------------------------------------

void SendSignal_DMA(void)
{

    uDMAChannelTransferSet(UDMA_CHANNEL_UART0TX | UDMA_PRI_SELECT,
                           UDMA_MODE_BASIC, signal,
                           (void *)(UART0_BASE + UART_O_DR), SIGNAL_SIZE * sizeof(uint32_t));

    uDMAChannelEnable(UDMA_CHANNEL_UART0TX);

}


// ---------------------------------------------
// Leitura da flag de transferência via uDMA
// ---------------------------------------------

uint32_t ReadFlag_DMA(void)
{

    uDMAChannelTransferSet(UDMA_CHANNEL_UART0RX | UDMA_PRI_SELECT,
                           UDMA_MODE_BASIC, (void *)(UART0_BASE + UART_O_DR),
                           &flag_transfer, sizeof(uint32_t));

    uDMAChannelEnable(UDMA_CHANNEL_UART0RX);

    while(uDMAChannelIsEnabled(UDMA_CHANNEL_UART0RX));

    return flag_transfer;

}

// ---------------------------------------------
// Interrupção do ADC
// ---------------------------------------------

void ADCSeqHandler(void)
{

    ADCIntClear(ADC0_BASE, ADC_SEQUENCER);

    while(!ADCIntStatus(ADC0_BASE, ADC_SEQUENCER, false)){}

    ADCSequenceDataGet(ADC0_BASE, ADC_SEQUENCER, &adcValue);

    signal[sampleIndex++] = adcValue;

    if (sampleIndex >= SIGNAL_SIZE)
    {
        SendSignal_DMA();
        sampleIndex = 0;

        ADCIntDisable(ADC0_BASE, ADC_SEQUENCER);
        IntDisable(INT_ADC0SS1);

    }
}


// ---------------------------------------------
// Main
// ---------------------------------------------
int main(void)
{

    ui32SysClkFreq = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);


    ConfigureTimer();

    ConfigureUART();

    ConfigureUDMA();

    ConfigureADC();

    IntMasterEnable();

    while(1)
    {
        flag_transfer = ReadFlag_DMA();

        if(flag_transfer)
        {
            ADCIntEnable(ADC0_BASE, ADC_SEQUENCER);
            IntEnable(INT_ADC0SS1);
        }

    }
}

