

/* Standard includes. */
#include <stdio.h>
#include <string.h>
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* Logging includes. */
#include "iot_logging_task.h"

/* MQTT include. */
#include "iot_mqtt_agent.h"

/* Required to get the broker address and port. */
#include "aws_clientcredential.h"
#include "queue.h"

/* Required for shadow API's */
#include "aws_shadow.h"
#include "jsmn.h"
#include "iot_init.h"

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "MK64F12.h"

/* Maximal count of tokens in parsed JSON */
#define MAX_CNT_TOKENS 32

#define shadowDemoTIMEOUT pdMS_TO_TICKS(30000UL)

/* Name of the thing */
#define shadowDemoTHING_NAME clientcredentialIOT_THING_NAME

#define shadowBUFFER_LENGTH 210

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/** stack size for task that handles shadow delta and updates
 */
#define TASK_STACK_SIZE ((uint16_t)configMINIMAL_STACK_SIZE * (uint16_t)20)

typedef struct
{
    char *pcDeltaDocument;
    uint32_t ulDocumentLength;
    void *xBuffer;
} jsonDelta_t;

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
 * Prototypes
 ******************************************************************************/
static void prvShadowMainTask(void *pvParameters);
static void canbus_task(void *pvParameters);

QueueHandle_t canbusQueue = NULL;
static TaskHandle_t awsHandle = NULL;
static TaskHandle_t canbusHandle = NULL;

/*******************************************************************************
 * Variables
 ******************************************************************************/
static char pcUpdateBuffer[shadowBUFFER_LENGTH];
static ShadowClientHandle_t xClientHandle;
QueueHandle_t jsonDeltaQueue = NULL;

uint8_t Tx_Frame(uint16_t ID, uint8_t *dataFrame, uint8_t msgBuffIndex);
uint8_t rxBuffer[8];
static volatile uint8_t CANFrameReceived = 0;
flexcan_frame_t txFrame, rxFrame;

uint8_t coolantTemp_Request[8] = {0x2,0x1,0x05,0x55,0x55,0x55,0x55,0x55};
uint8_t RPM_Request[8] = {0x2,0x1,0x0C,0x55,0x55,0x55,0x55,0x55};
uint8_t speed_Request[8] = {0x2,0x1,0x0D,0x55,0x55,0x55,0x55,0x55};
uint8_t MAF_Request[8] = {0x2,0x1,0x10,0x55,0x55,0x55,0x55,0x55};
uint8_t engineLoad_Request[8] = {0x2,0x1,0x04,0x55,0x55,0x55,0x55,0x55};

CAN_dataFrame[5] = {coolantTemp_Request, RPM_Request, speed_Request, MAF_Request, engineLoad_Request};

static uint8_t coolantTemp;
static uint16_t rpm;
static uint8_t speed;
static uint8_t maf;
static uint8_t load;

uint8_t count = 0;
uint8_t AWS_Init = 0;

uint16_t tempState = 0;
uint16_t rpmState = 0;
uint16_t speedState = 0;
uint16_t mafState = 0;
uint16_t loadState = 0;

uint16_t parsedtempState = 0;
uint16_t parsedrpmState = 0;
uint16_t parsedSpeedState = 0;
uint16_t parsedmafState = 0;
uint16_t parsedloadState = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Setup Rx Message Buffer. */
const flexcan_rx_mb_config_t rxMbConfig = {
  .id = FLEXCAN_ID_STD(0x7E8),
  .format = kFLEXCAN_FrameFormatStandard,
  .type = kFLEXCAN_FrameTypeData
};

/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {
	uint8_t x;

	printf("Message Buffer Status Flag: %d\n\r", FLEXCAN_GetMbStatusFlags(CAN0, 0x01));

	if(FLEXCAN_GetMbStatusFlags(CAN0, 0x01) == 1){
		FLEXCAN_ClearMbStatusFlags(CAN0, 0x01);
		FLEXCAN_ReadRxMb(CAN0, 0 , &rxFrame);

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
}

static void canbus_task(void *pvParameters) {
	while(1){
	 datastruct canbus_struct;
	 uint8_t i = 0;

		for(i = 0; i < 6; i++){

			if(i == 5){
				i = 0;
				vTaskSuspend(NULL);
				vTaskResume(awsHandle);
			}

			Tx_Frame(0x7DF, CAN_dataFrame[i], 1);

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
		}
	}
}

uint8_t Tx_Frame(uint16_t ID, uint8_t *dataFrame, uint8_t msgBuffIndex) {
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

/* Build JSON document with reported state of the "Coolant Temp" */
int buildJsonTEMP()
{
    char tmpBufcoolantTemp[128] = {0};

    sprintf(tmpBufcoolantTemp,  "{\"temp\":{\"temp\":%d}}", coolantTemp);

    int ret = 0;
    ret     = snprintf(pcUpdateBuffer, shadowBUFFER_LENGTH,
    		   "{\"state\":{"
			   "\"desired\":{"
			   "\"tempUpdate\":null"
			   "},"
			   "\"reported\":%s},"
			   "\"clientToken\": \"token-%d\""
			   "}",
			   tmpBufcoolantTemp, (int)xTaskGetTickCount());

    if (ret >= shadowBUFFER_LENGTH || ret < 0)
    {
        return -1;
    }
    else
    {
        return ret;
    }
}

/* Build JSON document with reported state of the "RPM" */
int buildJsonRPM()
{
    char tmpBufRPM[128] = {0};

    sprintf(tmpBufRPM,  "{\"rpm\":{\"rpm\":%d}}", rpm);

    int ret = 0;
    ret     = snprintf(pcUpdateBuffer, shadowBUFFER_LENGTH,
    		   "{\"state\":{"
			   "\"desired\":{"
			   "\"rpmUpdate\":null"
			   "},"
			   "\"reported\":%s},"
			   "\"clientToken\": \"token-%d\""
			   "}",
               tmpBufRPM, (int)xTaskGetTickCount());

    if (ret >= shadowBUFFER_LENGTH || ret < 0)
    {
        return -1;
    }
    else
    {
        return ret;
    }
}


/* Build JSON document with reported state of the "Speed" */
int buildJsonSpeed()
{

    char tmpBufSpeed[128] = {0};

    sprintf(tmpBufSpeed,  "{\"speed\":{\"speed\":%d}}", speed);

    int ret = 0;
    ret     = snprintf(pcUpdateBuffer, shadowBUFFER_LENGTH,
    		   "{\"state\":{"
    		   "\"desired\":{"
			   "\"speedUpdate\":null"
			   "},"
			   "\"reported\":%s},"
			   "\"clientToken\": \"token-%d\""
			   "}",
               tmpBufSpeed, (int)xTaskGetTickCount());

    if (ret >= shadowBUFFER_LENGTH || ret < 0)
    {
        return -1;
    }
    else
    {
        return ret;
    }
}

/* Build JSON document with reported state of the "MAF" */
int buildJsonMAF()
{

    char tmpBufMAF[128] = {0};

    sprintf(tmpBufMAF,  "{\"maf\":{\"maf\":%d}}", maf);

    int ret = 0;
    ret     = snprintf(pcUpdateBuffer, shadowBUFFER_LENGTH,
    		   "{\"state\":{"
    		   "\"desired\":{"
			   "\"mafUpdate\":null"
			   "},"
			   "\"reported\":%s},"
			   "\"clientToken\": \"token-%d\""
			   "}",
               tmpBufMAF, (int)xTaskGetTickCount());

    if (ret >= shadowBUFFER_LENGTH || ret < 0)
    {
        return -1;
    }
    else
    {
        return ret;
    }
}

/* Build JSON document with reported state of the "Engine Load" */
int buildJsonLOAD()
{

    char tmpBufLOAD[128] = {0};

    sprintf(tmpBufLOAD,  "{\"load\":{\"load\":%d}}", load);

    int ret = 0;
    ret     = snprintf(pcUpdateBuffer, shadowBUFFER_LENGTH,
    		   "{\"state\":{"
    		   "\"desired\":{"
			   "\"loadUpdate\":null"
			   "},"
			   "\"reported\":%s},"
			   "\"clientToken\": \"token-%d\""
			   "}",
               tmpBufLOAD, (int)xTaskGetTickCount());

    if (ret >= shadowBUFFER_LENGTH || ret < 0)
    {
        return -1;
    }
    else
    {
        return ret;
    }
}

/* Called when there's a difference between "reported" and "desired" in Shadow document. */
static BaseType_t prvDeltaCallback(void *pvUserData,
                                   const char *const pcThingName,
                                   const char *const pcDeltaDocument,
                                   uint32_t ulDocumentLength,
                                   MQTTBufferHandle_t xBuffer)
{
    (void)pvUserData;
    (void)pcThingName;

    /* add the to queue for processing */
    jsonDelta_t jsonDelta;
    jsonDelta.pcDeltaDocument  = (char *)pcDeltaDocument;
    jsonDelta.ulDocumentLength = ulDocumentLength;
    jsonDelta.xBuffer          = xBuffer;

    if (jsonDeltaQueue == NULL)
    {
        return pdFALSE;
    }

    if (xQueueSend(jsonDeltaQueue, &jsonDelta, (TickType_t)10) != pdPASS)
    {
        configPRINTF(("Fail to send message to jsonDeltaQueue.\r\n"));
        /* return pdFALSE - don't take ownership */
        return pdFALSE;
    }

    /* return pdTRUE - take ownership of the mqtt buffer - must be returned by SHADOW_ReturnMQTTBuffer */
    return pdTRUE;
}

/* Generate initial shadow document */
static uint32_t prvGenerateShadowJSON()
{
    /* Init shadow document with settings desired and reported state of device. */
    return snprintf(pcUpdateBuffer, shadowBUFFER_LENGTH,
                    "{"
                    "\"state\":{"
                    "\"desired\":{"
                    "\"tempstate\":%d"
                    "},"
                    "\"reported\":{"
                    "\"tempstate\":%d,"
    				"\"temp\":{\"temp\":0},"
    				"\"rpm\":{\"rpm\":0},"
    				"\"speed\":{\"speed\":0},"
    				"\"maf\":{\"maf\":0},"
    				"\"load\":{\"load\":0},"
                    "\"Tempinfo\":{"
                    "}"
                    "}"
                    "},"
                    "\"clientToken\": \"token-%d\""
                    "}",
                    tempState, tempState, (int)xTaskGetTickCount());
}

int parseStringValue(char *val, char *json, jsmntok_t *token)
{
    if (token->type == JSMN_STRING)
    {
        int len = token->end - token->start;
        memcpy(val, json + token->start, len);
        val[len] = '\0';
        return 0;
    }
    return -1;
}

int parseUInt16Value(uint16_t *val, char *json, jsmntok_t *token)
{
    if (token->type == JSMN_PRIMITIVE)
    {
        if (sscanf(json + token->start, "%hu", val) == 1)
        {
            return 0;
        }
    }
    return -1;
}

int compareString(char *json, jsmntok_t *token, char *string)
{
    if (token->type == JSMN_STRING)
    {
        int len    = strlen(string);
        int tokLen = token->end - token->start;
        if (len > tokLen)
        {
            len = tokLen;
        }
        return strncmp(string, json + token->start, len);
    }
    return -1;
}

int findTokenIndex(char *json, jsmntok_t *tokens, uint32_t length, char *name)
{
    int i;
    for (i = 0; i < length; i++)
    {
        if (compareString(json, &tokens[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* Process shadow delta JSON */
void processShadowDeltaJSON(char *json, uint32_t jsonLength)
{
    jsmn_parser parser;
    jsmn_init(&parser);

    jsmntok_t tokens[MAX_CNT_TOKENS];

    int tokenCnt = 0;
    tokenCnt     = jsmn_parse(&parser, json, jsonLength, tokens, MAX_CNT_TOKENS);
    /* the token with state of device is at 6th positin in this delta JSON:
     * {"version":229,"timestamp":1510062270,"state":{"LEDstate":1},"metadata":{"LEDstate":{"timestamp":1510062270}}} */
    if (tokenCnt < 7)
    {
        return;
    }

    /* find index of token "state" */
    int stateTokenIdx;
    stateTokenIdx = findTokenIndex(json, tokens, tokenCnt, "state");
    if (stateTokenIdx < 0)
    {
        return;
    }

    int i = stateTokenIdx + 1;

    /* end position of "state" object in JSON */
    int stateTokenEnd = tokens[i].end;

    char key[20]         = {0};
    int err              = 0;
    uint16_t parsedValue = 0;

    while (i < tokenCnt)
    {
        if (tokens[i].end > stateTokenEnd)
        {
            break;
        }

        err = parseStringValue(key, json, &tokens[i++]);
        if (err == 0)
        {
            if (strstr(key, "tempUpdate"))
            {
            	 /* found "updateAccel" keyword, parse value of next token */
                err = parseUInt16Value(&parsedValue, json, &tokens[i]);
                if (err == 0)
                {
                    parsedtempState = parsedValue;
                }
            }
            else if (strstr(key, "rpmUpdate"))
            {
                /* found "updateRpm" keyword, parse value of next token */
                err = parseUInt16Value(&parsedValue, json, &tokens[i]);
                if (err == 0)
                {
                    parsedrpmState = parsedValue;
                }
            } else if (strstr(key, "speedUpdate"))
            {
                /* found "updateRpm" keyword, parse value of next token */
                err = parseUInt16Value(&parsedValue, json, &tokens[i]);
                if (err == 0)
                {
                    parsedSpeedState = parsedValue;
                }
            } else if (strstr(key, "mafUpdate"))
            {
                /* found "updateMaf" keyword, parse value of next token */
                err = parseUInt16Value(&parsedValue, json, &tokens[i]);
                if (err == 0)
                {
                    parsedmafState = parsedValue;
                }
            } else if (strstr(key, "loadUpdate"))
            {
                /* found "updateMaf" keyword, parse value of next token */
                err = parseUInt16Value(&parsedValue, json, &tokens[i]);
                if (err == 0)
                {
                    parsedloadState = parsedValue;
                }
            }
            i++;
        }
    }
}

static ShadowReturnCode_t prvShadowClientCreateConnect(void)
{
    MQTTAgentConnectParams_t xConnectParams;
    ShadowCreateParams_t xCreateParams;
    ShadowReturnCode_t xReturn;

    xCreateParams.xMQTTClientType = eDedicatedMQTTClient;
    xReturn                       = SHADOW_ClientCreate(&xClientHandle, &xCreateParams);

    if (xReturn == eShadowSuccess)
    {
        memset(&xConnectParams, 0x00, sizeof(xConnectParams));
        xConnectParams.pcURL  = clientcredentialMQTT_BROKER_ENDPOINT;
        xConnectParams.usPort = clientcredentialMQTT_BROKER_PORT;

        xConnectParams.xFlags            = mqttagentREQUIRE_TLS;
        xConnectParams.pcCertificate     = NULL;
        xConnectParams.ulCertificateSize = 0;
        xConnectParams.pxCallback        = NULL;
        xConnectParams.pvUserData        = &xClientHandle;

        xConnectParams.pucClientId      = (const uint8_t *)(clientcredentialIOT_THING_NAME);
        xConnectParams.usClientIdLength = (uint16_t)strlen(clientcredentialIOT_THING_NAME);
        xReturn                         = SHADOW_ClientConnect(xClientHandle, &xConnectParams, shadowDemoTIMEOUT);

        if (xReturn != eShadowSuccess)
        {
            configPRINTF(("Shadow_ClientConnect unsuccessful, returned %d.\r\n", xReturn));
        }
    }
    else
    {
        configPRINTF(("Shadow_ClientCreate unsuccessful, returned %d.\r\n", xReturn));
    }

    return xReturn;
}

void prvShadowMainTask(void *pvParameters)
{
    (void)pvParameters;

    /* Initialize common libraries required by demo. */
    if (IotSdk_Init() != true)
    {
        configPRINTF(("Failed to initialize the common library."));
        vTaskDelete(NULL);
    }

    jsonDeltaQueue = xQueueCreate(8, sizeof(jsonDelta_t));
    if (jsonDeltaQueue == NULL)
    {
        configPRINTF(("Failed to create jsonDeltaQueue queue.\r\n"));
        vTaskDelete(NULL);
    }

    if (prvShadowClientCreateConnect() != eShadowSuccess)
    {
        configPRINTF(("Failed to initialize, stopping demo.\r\n"));
        vTaskDelete(NULL);
    }

    ShadowOperationParams_t xOperationParams;
    xOperationParams.pcThingName = shadowDemoTHING_NAME;
    xOperationParams.xQoS        = eMQTTQoS0;
    xOperationParams.pcData      = NULL;
    /* Don't keep subscriptions, since SHADOW_Delete is only called here once. */
    xOperationParams.ucKeepSubscriptions = pdFALSE;

    ShadowReturnCode_t xReturn;

    /* Delete the device shadow before initial update */
    xReturn = SHADOW_Delete(xClientHandle, &xOperationParams, shadowDemoTIMEOUT);

    /* Atttempting to delete a non-existant shadow returns eShadowRejectedNotFound.
     * Either eShadowSuccess or eShadowRejectedNotFound signify that there's no
     * existing Thing Shadow, so both values are ok. */
    if ((xReturn != eShadowSuccess) && (xReturn != eShadowRejectedNotFound))
    {
        configPRINTF(("Failed to delete device shadow, stopping demo.\r\n"));
        vTaskDelete(NULL);
    }

    /* Register callbacks. This demo doesn't use updated or deleted callbacks, so
     * those members are set to NULL. The callbacks are registered after deleting
     * the Shadow so that any previous Shadow doesn't unintentionally trigger the
     * delta callback.*/
    ShadowCallbackParams_t xCallbackParams;
    xCallbackParams.pcThingName            = shadowDemoTHING_NAME;
    xCallbackParams.xShadowUpdatedCallback = NULL;
    xCallbackParams.xShadowDeletedCallback = NULL;
    xCallbackParams.xShadowDeltaCallback   = prvDeltaCallback;

    xReturn = SHADOW_RegisterCallbacks(xClientHandle, &xCallbackParams, shadowDemoTIMEOUT);
    if (xReturn != eShadowSuccess)
    {
        configPRINTF(("Failed to register the delta callback, stopping demo.\r\n"));
        vTaskDelete(NULL);
    }

    printf("%s\n\r",pcUpdateBuffer);
    xOperationParams.pcData       = pcUpdateBuffer;
    xOperationParams.ulDataLength = prvGenerateShadowJSON();
    /* Keep subscriptions across multiple calls to SHADOW_Update. */
    xOperationParams.ucKeepSubscriptions = pdTRUE;

    /* create initial shadow document for the thing */
    if (SHADOW_Update(xClientHandle, &xOperationParams, shadowDemoTIMEOUT) != eShadowSuccess)
    {
        configPRINTF(("Failed to update device shadow, stopping demo.\r\n"));
        vTaskDelete(NULL);
    }

    configPRINTF(("AWS Remote Control Demo initialized.\r\n"));
    configPRINTF(("Use mobile application to control the remote device.\r\n"));

    jsonDelta_t jsonDelta;

    AWS_Init = 1;

    if(AWS_Init == 1){
    	vStartCanBusTask();
     }

    for (;;)
    {
    	AWS_Init = 0;

        /* process delta shadow JSON received in prvDeltaCallback() */

        if (xQueueReceive(jsonDeltaQueue, &jsonDelta, portMAX_DELAY) == pdTRUE)
        {
        	if(count == 1){
        		count = 0;
				vTaskResume(canbusHandle);
			}

            /* process item from queue */
            processShadowDeltaJSON(jsonDelta.pcDeltaDocument, jsonDelta.ulDocumentLength);

            if (parsedtempState == 1)
            {
            	count ++;
                configPRINTF(("Update Coolant Temperature.\r\n"));
                printf("%s\n\r", pcUpdateBuffer);
                xOperationParams.ulDataLength = buildJsonTEMP();
                xReturn                       = SHADOW_Update(xClientHandle, &xOperationParams, shadowDemoTIMEOUT);
                if (xReturn == eShadowSuccess)
                {
                    configPRINTF(("Successfully performed update.\r\n"));
                }
                else
                {
                    configPRINTF(("Update failed, returned %d.\r\n", xReturn));
                }
                parsedtempState = 0;
            }else if (parsedrpmState == 1)
            {
            	count ++;
                configPRINTF(("Update RPM Value.\r\n"));
                printf("%s\n\r", pcUpdateBuffer);
                xOperationParams.ulDataLength = buildJsonRPM();
                xReturn                       = SHADOW_Update(xClientHandle, &xOperationParams, shadowDemoTIMEOUT);
                if (xReturn == eShadowSuccess)
                {
                    configPRINTF(("Successfully performed update.\r\n"));
                }
                else
                {
                    configPRINTF(("Update failed, returned %d.\r\n", xReturn));
                }
                parsedrpmState = 0;
           }else if (parsedSpeedState == 1)
           {
        	   count ++;
               configPRINTF(("Update Speed Value.\r\n"));
               printf("%s\n\r", pcUpdateBuffer);
               xOperationParams.ulDataLength = buildJsonSpeed();
               xReturn                       = SHADOW_Update(xClientHandle, &xOperationParams, shadowDemoTIMEOUT);
               if (xReturn == eShadowSuccess)
               {
                   configPRINTF(("Successfully performed update.\r\n"));
               }
               else
               {
                   configPRINTF(("Update failed, returned %d.\r\n", xReturn));
               }
               parsedSpeedState = 0;
          }else if (parsedmafState == 1)
           {
        	   count ++;
               configPRINTF(("Update MAF Value.\r\n"));
               printf("%s\n\r", pcUpdateBuffer);
               xOperationParams.ulDataLength = buildJsonMAF();
               xReturn                       = SHADOW_Update(xClientHandle, &xOperationParams, shadowDemoTIMEOUT);
               if (xReturn == eShadowSuccess)
               {
                   configPRINTF(("Successfully performed update.\r\n"));
               }
               else
               {
                   configPRINTF(("Update failed, returned %d.\r\n", xReturn));
               }
               parsedmafState = 0;
          }else if (parsedloadState == 1)
          {
        	  count ++;
              configPRINTF(("Update Engine Load Value.\r\n"));
              printf("%s\n\r", pcUpdateBuffer);
              xOperationParams.ulDataLength = buildJsonLOAD();
              xReturn                       = SHADOW_Update(xClientHandle, &xOperationParams, shadowDemoTIMEOUT);
              if (xReturn == eShadowSuccess)
              {
                 configPRINTF(("Successfully performed update.\r\n"));
              }
              else
              {
                  configPRINTF(("Update failed, returned %d.\r\n", xReturn));
              }
              parsedloadState = 0;
         }
            /* return mqtt buffer */
            xReturn = SHADOW_ReturnMQTTBuffer(xClientHandle, jsonDelta.xBuffer);
            if (xReturn != eShadowSuccess)
            {
                configPRINTF(("Return MQTT buffer failed, returned %d.\r\n", xReturn));
            }
        }
    }
}

static void retrieveData_task(void *pvParameters) {
datastruct data_struct;

while(1) {
	if(xQueueReceive(canbusQueue, &data_struct, portMAX_DELAY) == pdTRUE){
			printf("ID: %d Data: %d \n\r",data_struct.sourceID, data_struct.dataField);

			switch(data_struct.sourceID){

			case coolTemp_ID:
				coolantTemp = data_struct.dataField;
				break;

			case RPM_ID:
				rpm = data_struct.dataField;
				break;

			case speed_ID:
				speed = data_struct.dataField;
				break;

			case MAF_ID:
				maf = data_struct.dataField;
				break;

			case engineLoad_ID:
				load = data_struct.dataField;
				break;

			default:
				break;
			}
		}
	}
}

void vStartTask(void)
{
    xTaskCreate(prvShadowMainTask, "AWS Task", TASK_STACK_SIZE, NULL, 3, &awsHandle);
}

void vStartCanBusTask(void)
{
	FLEXCAN_SetRxMbConfig(CAN0, 0 , &rxMbConfig, true);

    canbusQueue = xQueueCreate(5, sizeof(datastruct));
    vQueueAddToRegistry(canbusQueue, "Can Bus Queue");
  	xTaskCreate(canbus_task, "CanBus Task", 200, NULL, 4, &canbusHandle);
	xTaskCreate(retrieveData_task, "Retrieve Data Task", 150, NULL, 5, NULL);
}
