
#include <stdio.h>
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "MK64F12.h"

 /*******************************************************************************
  * Definitions
  ******************************************************************************/

 /*******************************************************************************
  * Prototypes
  ******************************************************************************/

 /*******************************************************************************
  * Variables
  ******************************************************************************/
 volatile bool rxComplete = false;
 flexcan_frame_t txFrame, rxFrame;

 /*******************************************************************************
  * Code
  ******************************************************************************/

/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {

	printf("Message Buffer Status Flags: %d\n\r", FLEXCAN_GetMbStatusFlags(CAN0, 0x01));

	if(FLEXCAN_GetMbStatusFlags(CAN0, 0x01) == 1){
		FLEXCAN_ClearMbStatusFlags(CAN0, 0x01);
		FLEXCAN_ReadRxMb(CAN0, 0 , &rxFrame);
		printf("rx word0 = 0x%x\r\n", rxFrame.dataWord0);
		printf("rx word1 = 0x%x\r\n", rxFrame.dataWord1);
		rxComplete = true;
}

  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}

int main(void) {

    flexcan_config_t flexcanConfig;
    flexcan_rx_mb_config_t mbConfig;

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    FLEXCAN_GetDefaultConfig(&flexcanConfig);

    /* Setup Rx Message Buffer. */
      mbConfig.format = kFLEXCAN_FrameFormatStandard;
      mbConfig.type   = kFLEXCAN_FrameTypeData;
      mbConfig.id     = FLEXCAN_ID_STD(0x7E8);
      FLEXCAN_SetRxMbConfig(CAN0, 0 , &mbConfig, true);
    // FLEXCAN_SetRxIndividualMask(CAN0, 0, 0X7FF<<18);

    /* Setup Tx Message Buffer. */
    //FLEXCAN_SetTxMbConfig(CAN0, 1, true);

    /* Prepare Tx Frame for sending. */
    txFrame.format = kFLEXCAN_FrameFormatStandard;
    txFrame.type   = kFLEXCAN_FrameTypeData;
    txFrame.id     = FLEXCAN_ID_STD(0x7DF);
    txFrame.length = 8;

    txFrame.dataWord0 = CAN_WORD0_DATA_BYTE_0(0x02) | CAN_WORD0_DATA_BYTE_1(0x01) | CAN_WORD0_DATA_BYTE_2(0x0D) |
            CAN_WORD0_DATA_BYTE_3(0x55);
    txFrame.dataWord1 = CAN_WORD1_DATA_BYTE_4(0x55) | CAN_WORD1_DATA_BYTE_5(0x55) | CAN_WORD1_DATA_BYTE_6(0x55) |
            CAN_WORD1_DATA_BYTE_7(0x55);

   /* txFrame.dataByte0 = 0x02;
    txFrame.dataByte1 = 0x01;
    txFrame.dataByte2 = 0x0D;
    txFrame.dataByte3 = 0x55;
    txFrame.dataByte4 = 0x55;
    txFrame.dataByte5 = 0x55;
    txFrame.dataByte6 = 0x55;
    txFrame.dataByte7 = 0x55;*/

    printf("Sending message\r\n");
    printf("tx word0 = 0x%x\r\n", txFrame.dataWord0);
    printf("tx word1 = 0x%x\r\n", txFrame.dataWord1);

    /* Send data through Tx Message Buffer using polling function. */
    FLEXCAN_TransferSendBlocking(CAN0, 1, &txFrame);

    /* Waiting for Message receive finish. */
	while (!rxComplete)
	{
	}

	printf("\r\nReceived message\r\n");

	printf("rx word0 = 0x%x\r\n", rxFrame.dataWord0);
	printf("rx word1 = 0x%x\r\n", rxFrame.dataWord1);
	//printf("rx = %d\r\n", &rxFrame);
    printf("TX Frame ID: %x",txFrame.id);
    printf("\n\rRX Frame ID: %x \n\r",rxFrame.id);

    while(1)
    {
    }

    return 0;
}
