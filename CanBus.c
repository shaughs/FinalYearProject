/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "MK64F12.h"

 flexcan_frame_t mbConfig;
 flexcan_frame_t txFrame,rxFrame;
 volatile bool rxComplete = false;

/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {

	printf("Message Buffer Status Flags: %x\n\r", FLEXCAN_GetMbStatusFlags(CAN0, 0x01));
	if(FLEXCAN_GetMbStatusFlags(CAN0, 0x01) == 1){
		FLEXCAN_ReadRxMb(CAN0, 0 , &rxFrame);
		rxComplete = true;
	}
	FLEXCAN_ClearMbStatusFlags(CAN0, 0x01);

  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}

int main(void) {

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif

    printf("Receive Frame Setup\n\r");
    mbConfig.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    mbConfig.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    //rxFrame.id     = (0x7DF);
    mbConfig.id     = (0x7E8<<18);


    printf("Transmit Frame Setup\n\r");
    txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    //txFrame.id     = (0x7E8);
    txFrame.id     = (0x7DF);
    txFrame.length = 8;

    txFrame.dataWord0 = CAN_WORD0_DATA_BYTE_0(0x02) | CAN_WORD0_DATA_BYTE_1(0x01) | CAN_WORD0_DATA_BYTE_2(0x0D) |
            CAN_WORD0_DATA_BYTE_3(0x55) | CAN_WORD1_DATA_BYTE_4(0x55) | CAN_WORD1_DATA_BYTE_5(0x55) | CAN_WORD1_DATA_BYTE_6(0x55) |
            CAN_WORD1_DATA_BYTE_7(0x55);
    txFrame.dataWord1 = CAN_WORD1_DATA_BYTE_4(0x55) | CAN_WORD1_DATA_BYTE_5(0x55) | CAN_WORD1_DATA_BYTE_6(0x55) |
            CAN_WORD1_DATA_BYTE_7(0x55);

    printf("Send message\r\n");
    printf("tx word0 = 0x%x\r\n", txFrame.dataWord0);
    printf("tx word1 = 0x%x\r\n", txFrame.dataWord1);

    FLEXCAN_TransferSendBlocking(CAN0, 1, &txFrame);

    /* Waiting for Message receive finish. */
	while (!rxComplete)
	{
	}

	printf("\r\nReceived message\r\n");

	printf("rx word0 = 0x%x\r\n", rxFrame.dataWord0);
	printf("rx word1 = 0x%x\r\n", rxFrame.dataWord1);
    printf("TX Frame ID: %x",txFrame.id);
    printf("\n\rRX Frame ID: %x \n\r",mbConfig.id);

    while(1){
    }

    return 0;
}
