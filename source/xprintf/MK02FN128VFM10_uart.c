#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK02F12810.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */
#include "fsl_uart.h"
#include "MK02FN128VFM10_uart.h"
#include "xprintf.h"

/* TODO: insert other definitions and declarations here. */
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*******************************************************************************
 * Variables
 ******************************************************************************/
UART_BUFF_T TxBuff;
UART_BUFF_T RxBuff;

/*******************************************************************************
 * Code
 ******************************************************************************/
void UART0_PutByte (uint8_t d)
{
	int32_t i;

	/* Wait for Tx buffer ready */
	while (TxBuff.ct >= BUFF_SIZE) ;

	__disable_irq();
	if(TxBuff.act){
		i = TxBuff.wi;		/* Put a byte into Tx byffer */
	  	TxBuff.buff[i++] = d;
	  	TxBuff.wi = i % BUFF_SIZE;
	  	TxBuff.ct++;
	} else {
	  	TxBuff.act = 1;		/* Trigger Tx sequense */
	  	UART_WriteByte(UART0, d);
	}
	__enable_irq();
	UART_EnableInterrupts(UART0, kUART_TransmissionCompleteInterruptEnable);
}

uint8_t UART0_GetByte (void)
{
	uint8_t d;
	int i;

	/* Wait while Rx buffer is empty */
	while (!RxBuff.ct) ;

	i = RxBuff.ri;			/* Get a byte from Rx buffer */
	d = RxBuff.buff[i++];
	RxBuff.ri = i % BUFF_SIZE;
	__disable_irq();
	RxBuff.ct--;
	__enable_irq();

	return d;
}

/* UART0_RX_TX_IRQn interrupt handler */
void UART0_SERIAL_RX_TX_IRQHANDLER(void) {
	uint32_t intStatus;
	uint8_t d;
	int i, cnt;

  /* Reading all interrupt flags of status registers */
  intStatus = UART_GetStatusFlags(UART0_PERIPHERAL);

  /* Flags can be cleared by reading the status register and reading/writing data registers.
    See the reference manual for details of each flag.
    The UART_ClearStatusFlags() function can be also used for clearing of flags in case the content of data regsiter is not used.
    For example:
        status_t status;
        intStatus &= ~(kUART_RxOverrunFlag | kUART_NoiseErrorFlag | kUART_FramingErrorFlag | kUART_ParityErrorFlag);
        status = UART_ClearStatusFlags(UART0_PERIPHERAL, intStatus);
  */

  /* Place your code here */
  /* If new data arrived. */
  if ((kUART_RxDataRegFullFlag | kUART_RxOverrunFlag) & intStatus) {
      i = RxBuff.wi;
      cnt = RxBuff.ct;
//        while (Chip_UART_ReadLineStatus(LPC_USART) & UART_LSR_RDR) {	/* Get all data in the Rx FIFO */
      	d = UART_ReadByte(UART0);
      	if (cnt < BUFF_SIZE) {	/* Store data if Rx buffer is not full */
      		RxBuff.buff[i++] = d;
      		i %= BUFF_SIZE;
      		cnt++;
      	}
//        }
     RxBuff.wi = i;
     RxBuff.ct = cnt;
     UART_ClearStatusFlags(UART0, kUART_RxDataRegFullFlag | kUART_RxOverrunFlag);
  }

//    if ((kUART_TxDataRegEmptyFlag | kUART_TransmissionCompleteFlag) & uart_flag) {
 if ((kUART_TransmissionCompleteFlag) & intStatus) {
		cnt = TxBuff.ct;
		if(cnt){/* There is one or more byte to send */
			i = TxBuff.ri;
			for (d = 16; d && cnt; d--, cnt--){	/* Fill Tx FIFO */
				UART_WriteByte(UART0, TxBuff.buff[i++]);
				i %= BUFF_SIZE;
			}
			TxBuff.ri = i;
			TxBuff.ct = cnt;
		}else{
			TxBuff.act = 0; /* When no data to send, next putc() must trigger Tx sequense */
			UART_DisableInterrupts(UART0, kUART_TransmissionCompleteInterruptEnable);
		}
	   UART_ClearStatusFlags(UART0, kUART_TransmissionCompleteFlag);
  }

  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}
