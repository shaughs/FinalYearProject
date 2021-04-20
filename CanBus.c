
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
 uint8_t CAN_Tx_Frame(uint16_t ID, uint8_t *dataFrame, uint8_t msgBuffIndex);
 uint8_t rxBuffer[8];
 static volatile uint8_t frameReceived = 0;
 flexcan_frame_t txFrame, rxFrame;

 static uint8_t speedData = 0;

 /*******************************************************************************
  * Code
  ******************************************************************************/

/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {
	uint8_t x;

	printf("Message Buffer Status Flags: %d\n\r", FLEXCAN_GetMbStatusFlags(CAN0, 0x01));

	if(FLEXCAN_GetMbStatusFlags(CAN0, 0x01) == 1){
		FLEXCAN_ClearMbStatusFlags(CAN0, 0x01);
		FLEXCAN_ReadRxMb(CAN0, 0 , &rxFrame);
		printf("Frame Received, ID: 0x%08x\n\r", (rxFrame.id & 0x1FFC0000)>>18);
		rxBuffer[0] = rxFrame.dataByte0;
		rxBuffer[1] = rxFrame.dataByte1;
		rxBuffer[2] = rxFrame.dataByte2;
		rxBuffer[3] = rxFrame.dataByte3;
		rxBuffer[4] = rxFrame.dataByte4;
		rxBuffer[5] = rxFrame.dataByte5;
		rxBuffer[6] = rxFrame.dataByte6;
		rxBuffer[7] = rxFrame.dataByte7;
		for(x=0;x<8;x++) {
			printf("%02x\t", rxBuffer[x]);
		}
		printf("\n\n\r");
		frameReceived = 1;
}

  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}

/* Setup Rx Message Buffer. */
const flexcan_rx_mb_config_t rxMbConfig = {
  .id = FLEXCAN_ID_STD(0x7E8),
  .format = kFLEXCAN_FrameFormatStandard,
  .type = kFLEXCAN_FrameTypeData
};

int main(void) {

	//uint8_t throttlePos_Request[8] = {0x2,0x1,0x11,0x55,0x55,0x55,0x55,0x55};
	uint8_t RPM_Request[8] = {0x2,0x1,0x0C,0x55,0x55,0x55,0x55,0x55};
	uint8_t speedRequest[8] = {0x2,0x1,0x0D,0x55,0x55,0x55,0x55,0x55};
	uint8_t MAF_Request[8] = {0x2,0x1,0x10,0x55,0x55,0x55,0x55,0x55};
	uint8_t engineLoad_Request[8] = {0x2,0x1,0x04,0x55,0x55,0x55,0x55,0x55};

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    FLEXCAN_SetRxMbConfig(CAN0, 0 , &rxMbConfig, true);
    // FLEXCAN_SetRxIndividualMask(CAN0, 0, 0X7FF<<18);

    /* Setup Tx Message Buffer. */
    //FLEXCAN_SetTxMbConfig(CAN0, 1, true);

    /* Prepare Tx Frame for sending. */
    txFrame.format = kFLEXCAN_FrameFormatStandard;
    txFrame.type   = kFLEXCAN_FrameTypeData;
    txFrame.id     = FLEXCAN_ID_STD(0x7DF);
    txFrame.length = 8;

    //txFrame.dataByte0 = 0x02;
   // txFrame.dataByte1 = 0x01;
   // txFrame.dataByte2 = 0x0D;
   // txFrame.dataByte3 = 0x55;
   // txFrame.dataByte4 = 0x55;
   // txFrame.dataByte5 = 0x55;
    //txFrame.dataByte6 = 0x55;
    //txFrame.dataByte7 = 0x55;

    //printf("Sending message\r\n");
 //   printf("tx word0 = 0x%x\r\n", txFrame.dataWord0);
  //  printf("tx word1 = 0x%x\r\n", txFrame.dataWord1);

    /* Send data through Tx Message Buffer using polling function. */
  //  FLEXCAN_TransferSendBlocking(CAN0, 1, &txFrame);

    while(1)
    {
    	CAN_Tx_Frame(0x7DF, speedRequest, 1);
    	while(frameReceived == 0);
    	frameReceived = 0;
    	switch(rxBuffer[2]){
    	case 0x0D:
    		speedData = rxBuffer[3];
    		printf("Speed: %dkm/h\n\n\r", rxBuffer[3]);
    		printf("Speed Data: %dkm/h\n\n\r", speedData);
    		break;
    	case 0x0C:
    		printf("RPM: %d\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4); break;
    	default: break;
    	}
    	CAN_Tx_Frame(0x7DF, RPM_Request, 1);
    	while(frameReceived == 0);
    	frameReceived = 0;
    	switch(rxBuffer[2]){
    	case 0x0D: printf("Speed: %dkm/h\n\n\r", rxBuffer[3]); break;
    	case 0x0C: printf("RPM: %d\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4); break;
    	default: break;
    	}
    	CAN_Tx_Frame(0x7DF, MAF_Request, 1);
		while(frameReceived == 0);
		frameReceived = 0;
		switch(rxBuffer[2]){
		case 0x0D: printf("Speed: %dkm/h\n\n\r", rxBuffer[3]); break;
		case 0x0C: printf("RPM: %d rpm\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4); break;
		case 0x10: printf("MAF: %d grams/sec\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/100); break;
		default: break;
		}
	   	CAN_Tx_Frame(0x7DF, engineLoad_Request, 1);
		while(frameReceived == 0);
		frameReceived = 0;
		switch(rxBuffer[2]){
		case 0x0D: printf("Speed: %dkm/h\n\n\r", rxBuffer[3]); break;
		case 0x0C: printf("RPM: %d rpm\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4); break;
		case 0x10: printf("MAF: %d grams/sec\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/100); break;
		case 0x04: printf("Engine Load: %d %\n\n\r", ((100/255)*(rxBuffer[3]))); break;
		default: break;
		}
	  	/*CAN_Tx_Frame(0x7DF, throttlePos_Request, 1);
		while(frameReceived == 0);
		frameReceived = 0;
		switch(rxBuffer[2]){
		case 0x0D: printf("Speed: %dkm/h\n\n\r", rxBuffer[3]); break;
		case 0x0C: printf("RPM: %d rpm\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4); break;
		case 0x10: printf("MAF: %d grams/sec\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/100); break;
		case 0x04: printf("Engine Load: %d \n\n\r", ((100/255)*(rxBuffer[3]))); break;
		case 0x11: printf("Throttle Position: %d \n\n\r", ((100/255)*(rxBuffer[3]))); break;
		default: break;
		}*/
    	for(uint32_t z=0; z<20000000; z++);
    }
    return 0 ;
}

uint8_t CAN_Tx_Frame(uint16_t ID, uint8_t *dataFrame, uint8_t msgBuffIndex) {
	flexcan_frame_t txFrame;
	txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    txFrame.id     = FLEXCAN_ID_STD(ID);
    txFrame.length = 8;
    txFrame.dataByte0 = dataFrame[0];
    txFrame.dataByte1 = dataFrame[1];
    txFrame.dataByte2 = dataFrame[2];
    txFrame.dataByte3 = dataFrame[3];
    txFrame.dataByte4 = dataFrame[4];
    txFrame.dataByte5 = dataFrame[5];
    txFrame.dataByte6 = dataFrame[6];
    txFrame.dataByte7 = dataFrame[7];
    printf("Sending CAN Frame\n\r");
    frameReceived = 0;
    FLEXCAN_TransferSendBlocking(CAN0, msgBuffIndex, &txFrame);
    printf("Frame Sent\n\r");
    return 1;
}
