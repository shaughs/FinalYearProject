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

 flexcan_frame_t rxFrame;
 flexcan_frame_t txFrame;

/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {
	uint32_t mask;
  /*  Place your code here */
	mask = FLEXCAN_GetMbStatusFlags(CAN0,0x03);
	printf("Status Flags: %x\n\r", mask);
	FLEXCAN_ClearMbStatusFlags(CAN0, mask);

	if(mask == 1){
		FLEXCAN_ReadRxMb(CAN0, 0, &rxFrame);
		printf("0x%02x\n\r", rxFrame.dataWord0);
		printf("0x%02x\n\r", rxFrame.dataWord1);
	}
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
    rxFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    rxFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    //rxFrame.id     = (0x7DF);
    rxFrame.id     = (0x7E8<<18);
    FLEXCAN_SetRxMbConfig(CAN0, 0x7FF<<18 , &rxFrame, true);

    printf("Transmit Frame Setup\n\r");
    txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    txFrame.id     = (0x7E8);
    txFrame.length = 8;

    txFrame.dataWord0 = CAN_WORD0_DATA_BYTE_0(0x02) | CAN_WORD0_DATA_BYTE_1(0x01) | CAN_WORD0_DATA_BYTE_2(0x0D) |
            CAN_WORD0_DATA_BYTE_3(0x55);
    txFrame.dataWord1 = CAN_WORD1_DATA_BYTE_4(0x55) | CAN_WORD1_DATA_BYTE_5(0x55) | CAN_WORD1_DATA_BYTE_6(0x55) |
            CAN_WORD1_DATA_BYTE_7(0x55);

    FLEXCAN_TransferSendBlocking(CAN0, 1, &txFrame);

    printf("Complete\n\r");
    printf("TX Frame ID: %x",txFrame.id);
    printf("\n\rRX Frame ID: %x \n\r",rxFrame.id);

    while(1){
    }


    return 0;
}
