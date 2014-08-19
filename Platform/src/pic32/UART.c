#ifndef __UART_C
#define __UART_C
#include "arch/pic32/BowlerConfig.h"
#include "Bowler/Bowler.h"

boolean useUart2 = FALSE;
boolean useUart1 = FALSE;

#define delayUart 5000

boolean Write32UART2(uint8_t data)
{
	int tick =delayUart;
	while (!UARTTransmitterIsReady(UART2)){
                if(INTGetFlag(INT_SOURCE_UART_ERROR(UART2))){
                    INTClearFlag(INT_SOURCE_UART_ERROR(UART2));
                }
		if(tick--==0){
			return FALSE;
		}
	}
	UARTSendDataByte(UART2, data);
	return TRUE;
}


boolean Write32UART1(uint8_t  data)
{

    int tick =delayUart;
	while (!UARTTransmitterIsReady(UART1)){
            if(INTGetFlag(INT_SOURCE_UART_ERROR(UART1))){
		INTClearFlag(INT_SOURCE_UART_ERROR(UART1));
            }
            if(tick--==0){
                return FALSE;
            }
	}
	UARTSendDataByte(UART1, data);
	return TRUE;
}

#endif	//STACK_USE_UART

