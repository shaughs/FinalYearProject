
///////////////////////////////////////////////////////////////////////////////
//  Includes
///////////////////////////////////////////////////////////////////////////////
/* SDK Included Files */
#include "board.h"
#include "fsl_debug_console.h"
#include "ksdk_mbedtls.h"
#include "pin_mux.h"
#include "peripherals.h"

/* FreeRTOS Demo Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "iot_logging_task.h"
#include "iot_system_init.h"
#include "aws_dev_mode_key_provisioning.h"
#include "platform/iot_threads.h"
#include "types/iot_network_types.h"

#include "aws_clientcredential.h"
#include "iot_wifi.h"
#include "fsl_device_registers.h"
#include "fsl_port.h"
#include "clock_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define INIT_SUCCESS 0
#define INIT_FAIL    1

#define LOGGING_TASK_PRIORITY   (tskIDLE_PRIORITY + 1)
#define LOGGING_TASK_STACK_SIZE (200)
#define LOGGING_QUEUE_LENGTH    (16)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

extern int initNetwork(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
const WIFINetworkParams_t pxNetworkParams = {
    .pcSSID           = clientcredentialWIFI_SSID,
    .ucSSIDLength     = sizeof(clientcredentialWIFI_SSID) - 1,
    .pcPassword       = clientcredentialWIFI_PASSWORD,
    .ucPasswordLength = sizeof(clientcredentialWIFI_PASSWORD) - 1,
    .xSecurity        = clientcredentialWIFI_SECURITY,
};

/*******************************************************************************
 * Code
 ******************************************************************************/



int initNetwork(void)
{
    WIFIReturnCode_t result;

    configPRINTF(("Starting WiFi...\r\n"));

    result = WIFI_On();
    if (result != eWiFiSuccess)
    {
        configPRINTF(("Could not enable WiFi, reason %d.\r\n", result));
        return INIT_FAIL;
    }

    configPRINTF(("WiFi module initialized.\r\n"));

    result = WIFI_ConnectAP(&pxNetworkParams);
    if (result != eWiFiSuccess)
    {
        configPRINTF(("Could not connect to WiFi, reason %d.\r\n", result));
        return INIT_FAIL;
    }

    configPRINTF(("WiFi connected to AP %s.\r\n", pxNetworkParams.pcSSID));

    uint8_t tmp_ip[4] = {0};
    result            = WIFI_GetIP(tmp_ip);

    if (result != eWiFiSuccess)
    {
        configPRINTF(("Could not get IP address, reason %d.\r\n", result));
        return INIT_FAIL;
    }

    configPRINTF(("IP Address acquired %d.%d.%d.%d\r\n", tmp_ip[0], tmp_ip[1], tmp_ip[2], tmp_ip[3]));

    return INIT_SUCCESS;
}


void main_task(void *pvParameters)
{
    if (SYSTEM_Init() == pdPASS)
    {
        if (initNetwork() != 0)
        {
            configPRINTF(("Network init failed, stopping demo.\r\n"));
            vTaskDelete(NULL);
        }
        else
        {
         vStartTask();
        }
    }
    vTaskDelete(NULL);
}

int main(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    BOARD_InitBootPeripherals();
    CRYPTO_InitHardware();

    if (xTaskCreate(main_task, "main_task", configMINIMAL_STACK_SIZE * 8, NULL, 4 , NULL) != pdPASS)
      {
          PRINTF("Main task creation failed!.\r\n");
          while (1);
      }

    xLoggingTaskInitialize(LOGGING_TASK_STACK_SIZE, LOGGING_TASK_PRIORITY, LOGGING_QUEUE_LENGTH);

    vTaskStartScheduler();

    while(1) {
    }
 return 0 ;
}

void print_string(const char *string)
{
    PRINTF(string);
}

void *pvPortCalloc(size_t xNum, size_t xSize)
{
    void *pvReturn;

    pvReturn = pvPortMalloc(xNum * xSize);
    if (pvReturn != NULL)
    {
        memset(pvReturn, 0x00, xNum * xSize);
    }

    return pvReturn;
}
