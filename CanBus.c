
#include <stdio.h>
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "MK64F12.h"
#include "queue.h"

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
 static volatile uint8_t CANFrameReceived = 0;
 flexcan_frame_t txFrame, rxFrame;

uint8_t coolantTemp_Request[8] = {0x2,0x1,0x05,0x55,0x55,0x55,0x55,0x55};
uint8_t RPM_Request[8] = {0x2,0x1,0x0C,0x55,0x55,0x55,0x55,0x55};
uint8_t speed_Request[8] = {0x2,0x1,0x0D,0x55,0x55,0x55,0x55,0x55};
uint8_t MAF_Request[8] = {0x2,0x1,0x10,0x55,0x55,0x55,0x55,0x55};
uint8_t engineLoad_Request[8] = {0x2,0x1,0x04,0x55,0x55,0x55,0x55,0x55};

Can_addresses[5] = {coolantTemp_Request, RPM_Request, speed_Request, MAF_Request, engineLoad_Request};

static void canbus_task(void *pvParameters);
static void display_task(void *pvParameters);
QueueHandle_t canbusQueue = NULL;

typedef struct {
	uint8_t sourceID;
	uint32_t dataField;
}datastruct;

#define coolTemp_ID 0
#define RPM_ID 1
#define speed_ID 2
#define MAF_ID 3
#define engineLoad_ID 4

 /*******************************************************************************
  * Code
  ******************************************************************************/

/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {
	uint8_t x;

	printf("Message Buffer Status Flag: %d\n\r", FLEXCAN_GetMbStatusFlags(CAN0, 0x01));

	if(FLEXCAN_GetMbStatusFlags(CAN0, 0x01) == 1){
		FLEXCAN_ClearMbStatusFlags(CAN0, 0x01);
		FLEXCAN_ReadRxMb(CAN0, 0 , &rxFrame);

		printf("\rCAN Rx Frame Received, ID: 0x%08x\n\r", (rxFrame.id & 0x1FFC0000)>>18);

		rxBuffer[0] = rxFrame.dataByte0;
		rxBuffer[1] = rxFrame.dataByte1;
		rxBuffer[2] = rxFrame.dataByte2;
		rxBuffer[3] = rxFrame.dataByte3;
		rxBuffer[4] = rxFrame.dataByte4;
		rxBuffer[5] = rxFrame.dataByte5;
		rxBuffer[6] = rxFrame.dataByte6;
		rxBuffer[7] = rxFrame.dataByte7;
		for(x = 0; x < 8; x++) {
			printf("%02x\t", rxBuffer[x]);
		}
		printf("\n\n\r");
		CANFrameReceived = 1;
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

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    FLEXCAN_SetRxMbConfig(CAN0, 0 , &rxMbConfig, true);

    canbusQueue = xQueueCreate(5, sizeof(datastruct));
	vQueueAddToRegistry(canbusQueue, "Can Bus Queue");
	xTaskCreate(canbus_task, "CanBus Task", 200, NULL, 3, NULL);
	xTaskCreate(display_task, "Display Task", 150, NULL, 4, NULL);
	vTaskStartScheduler();

    while(1) {
     }
	return 0 ;
}


static void canbus_task(void *pvParameters) {
	while(1){
	 datastruct canbus_struct;
	 uint8_t i = 0;

		for(i = 0; i < 6; i++){

			if(i == 5){
				i = 0;
			}

			CAN_Tx_Frame(0x7DF, Can_addresses[i], 1);

			while(CANFrameReceived == 0);
			CANFrameReceived = 0;

			switch(rxBuffer[2]){
			case 0x05:
				printf("Coolant Temperature: %d Degrees Celsius\n\n\r", (rxBuffer[3]));
				canbus_struct.sourceID = 0;
				canbus_struct.dataField = (rxBuffer[3]);
				xQueueSend(canbusQueue, &canbus_struct, 0);
				break;

			case 0x0C:
				printf("RPM: %d revolutions per minute\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4);
				canbus_struct.sourceID = 1;
				canbus_struct.dataField = (((uint16_t)rxBuffer[3]*256+rxBuffer[4])/4);
				xQueueSend(canbusQueue, &canbus_struct, 0);
				break;

			case 0x0D:
				printf("Speed: %dkm/h\n\n\r", rxBuffer[3]);
				canbus_struct.sourceID = 2;
				canbus_struct.dataField = (rxBuffer[3]);
				xQueueSend(canbusQueue, &canbus_struct, 0);
				break;

			case 0x10:
				printf("MAF: %d grams/sec\n\n\r", ((uint16_t)rxBuffer[3]*256+rxBuffer[4])/100);
				canbus_struct.sourceID = 3;
				canbus_struct.dataField = (((uint16_t)rxBuffer[3]*256+rxBuffer[4])/100);
				xQueueSend(canbusQueue, &canbus_struct, 0);
				break;

			case 0x04:
				printf("Engine Load: %d %%\n\n\r", ((uint16_t)rxBuffer[3])*100/255);
				canbus_struct.sourceID = 4;
				canbus_struct.dataField = (((uint16_t)rxBuffer[3])*100/255);
				xQueueSend(canbusQueue, &canbus_struct, 0);
				break;

			default:
				break;
			}
			for(uint32_t z=0; z<2000000; z++);
		}
	}
}

static void display_task(void *pvParameters) {
	datastruct display_struct;
	printf("Starting Display Task\n\r");
	while(1) {
		if(xQueueReceive(canbusQueue, &display_struct, portMAX_DELAY) == pdTRUE){
			printf("ID: %d Data: %d \n\r",display_struct.sourceID, display_struct.dataField);
		}
	}
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

    CANFrameReceived = 0;

    printf("\rSending CAN Tx Frame\n\r");

    FLEXCAN_TransferSendBlocking(CAN0, msgBuffIndex, &txFrame);\

    printf("CAN TX Frame Sent\n\r");
    return 1;
}
