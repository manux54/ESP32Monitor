#include <stdio.h>
#include <stdint.h>

#include "esp_wifi.h"
#include "esp_event_loop.h" 
#include "esp_log.h"

#include "iothub_client.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "iothubtransportmqtt.h"

#include "cJSON.h"

#include "driver/gpio.h"

#include "iot-hub.h"

#define DEFAULT_REFRESH_RATE 30000

/* Logging Tag */
typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;  // For tracking the messages within the user callback.
} EVENT_INSTANCE;

static const char *IOT_TAG = "iot-hub";
static const char *_deviceId;

static unsigned int _desiredRefreshRate = DEFAULT_REFRESH_RATE;

IOTHUB_CLIENT_LL_HANDLE _iotHubClientHandle = NULL;

unsigned int iothub_refreshRate()
{
    return _desiredRefreshRate;
}

void ReportedStateCallback(int status_code, void * userContextCallback)
{
    ESP_LOGI(IOT_TAG, "Reported State Callback with status code %d", status_code)
}

esp_err_t iothub_reportTwinData(int refreshRate)
{
    char data[100];
    sprintf(data, "{\"refreshRate\":\"%u\"}", refreshRate);

    if(IoTHubClient_LL_SendReportedState(_iotHubClientHandle, (unsigned char *) data, strlen(data), ReportedStateCallback, NULL) != IOTHUB_CLIENT_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}


static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    int* counter = (int*)userContextCallback;
    const char* buffer;
    size_t size;
    MAP_HANDLE mapProperties;
    const char* messageId;
    const char* correlationId;

    // Message properties
    if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL)
    {
        messageId = "<null>";
    }

    if ((correlationId = IoTHubMessage_GetCorrelationId(message)) == NULL)
    {
        correlationId = "<null>";
    }

    // Message content
    if (IoTHubMessage_GetByteArray(message, (const unsigned char**)&buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        ESP_LOGE(IOT_TAG, "Failed to retrieve the message data\n");
    }
    else
    {
        ESP_LOGI(IOT_TAG, "Received Message [%d]\n Message ID: %s\n Correlation ID: %s\n Data: <<<%.*s>>> & Size=%d\n", 
            *counter, 
            messageId, 
            correlationId, 
            (int)size, 
            buffer, 
            (int)size);
    }

    // Retrieve properties from the message
    mapProperties = IoTHubMessage_Properties(message);

    if (mapProperties != NULL)
    {
        const char*const* keys;
        const char*const* values;
        size_t propertyCount = 0;
        if (Map_GetInternals(mapProperties, &keys, &values, &propertyCount) == MAP_OK)
        {
            if (propertyCount > 0)
            {
                size_t index;

                ESP_LOGI(IOT_TAG, " Message Properties:\n");

                for (index = 0; index < propertyCount; index++)
                {
                    ESP_LOGI(IOT_TAG, "\tKey: %s Value: %s\n", keys[index], values[index]);
                }
                ESP_LOGI(IOT_TAG, "\n");
            }
        }
    }

    /* Some device specific action code goes here... */
    (*counter)++;
    return IOTHUBMESSAGE_ACCEPTED;
}

static int ToggleLight(bool on, unsigned char** response, size_t* resp_size)
{
    char * responseString;

    if (on)
    {
        gpio_set_level(2, 1);
        responseString = "{ \"Response\": \"Light turned on\" }";
    }
    else
    {
        gpio_set_level(2, 0);
        responseString = "{ \"Response\": \"Light turned off\" }";
    }

    ESP_LOGI(IOT_TAG, "%s", responseString);
    
    int status = 200;

    *resp_size = strlen(responseString);

    if ((*response = malloc(*resp_size)) == NULL)
    {
        status = -1;
    }
    else
    {
        (void)memcpy(*response, responseString, *resp_size);
    }

    return status;
}


static int DeviceMethodCallback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* resp_size, void* userContextCallback)
{
    if(strcasecmp(method_name, "togglelight") == 0)
    {
        cJSON * root = cJSON_Parse( (const char *)payload);
        bool light = cJSON_GetObjectItem(root, "on")->valueint == 1;
        cJSON_Delete(root);

        return ToggleLight(light, response, resp_size);
    }
    return -1;
}

void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    EVENT_INSTANCE* eventInstance = (EVENT_INSTANCE*)userContextCallback;

    ESP_LOGI(IOT_TAG, "Confirmation received for message tracking id = %zu with result = %s\n", 
        eventInstance->messageTrackingId, 
        ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

    /* Some device specific action code goes here... */

    IoTHubMessage_Destroy(eventInstance->messageHandle);
}

esp_err_t iothub_init(const char * hostname, const char * deviceId, const char * primaryKey)
{
    _deviceId = deviceId;

    int receiveContext = 0;
    
    if (platform_init() != 0)
    {
        ESP_LOGE(IOT_TAG, "Failed to initialize the platform\n");
        return ESP_FAIL;
    }

    char connectionString[256];

    if (sprintf_s(connectionString, sizeof(connectionString), "HostName=%s;DeviceId=%s;SharedAccessKey=%s",
        hostname, deviceId, primaryKey) <= 0) 
    {
        ESP_LOGE(IOT_TAG, "Failed to create the connection string\n");
        return ESP_FAIL;
    }
    
    _iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);    

    if (_iotHubClientHandle == NULL)
    {
        ESP_LOGE(IOT_TAG, "Failed to create the IoT Hub connection\n");
        return ESP_FAIL;
    }

    bool traceOn = true;
    IoTHubClient_LL_SetOption(_iotHubClientHandle, "logtrace", &traceOn);

    /* Setting Message call back, so we can receive Commands. */
    if (IoTHubClient_LL_SetMessageCallback(_iotHubClientHandle, ReceiveMessageCallback, &receiveContext) != IOTHUB_CLIENT_OK)
    {
        (void)printf("ERROR: IoTHubClient_LL_SetMessageCallback..........FAILED!\r\n");
    }

    /* Register Device Method */
    if (IoTHubClient_LL_SetDeviceMethodCallback(_iotHubClientHandle, DeviceMethodCallback, NULL) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(IOT_TAG, "Failed to Register Toggle Light Device Method");
    }
    
    return ESP_OK;
}

esp_err_t iothub_processCensorData(const float temperature)
{
    EVENT_INSTANCE message;
    static int messageCounter;

    char msgText[100];
    sprintf(msgText, "{\"deviceId\":\"%s\",\"temperature\":%.4f}", _deviceId, temperature);

    message.messageTrackingId = ++messageCounter;
    message.messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText));

    if (message.messageHandle == NULL)
    {
        ESP_LOGE(IOT_TAG, "IoT Message creation failed\n");
        return ESP_FAIL;
    }

    IoTHubMessage_SetMessageId(message.messageHandle, "MSG_ID");
    IoTHubMessage_SetCorrelationId(message.messageHandle, "CORE_ID");

    MAP_HANDLE propMap = IoTHubMessage_Properties(message.messageHandle);
    if (Map_AddOrUpdate(propMap, "temperatureAlert", temperature > 28 ? "true" : "false") != MAP_OK)
    {
        ESP_LOGE(IOT_TAG, "Failed to create temperature alert property");
    }

    if (IoTHubClient_LL_SendEventAsync(_iotHubClientHandle, message.messageHandle, SendConfirmationCallback, &message) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(IOT_TAG, "IoTHubClient_LL_SendEventAsync Failed\n");
        return ESP_FAIL;
    }

    ESP_LOGI(IOT_TAG, "IoTHubClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\n", messageCounter);
    ESP_LOGI(IOT_TAG, "Message: %s", msgText);

    return ESP_OK;
}

esp_err_t iothub_processEventQueue()
{
    IOTHUB_CLIENT_STATUS status;

    do    
    {
        IoTHubClient_LL_DoWork(_iotHubClientHandle);
    }
    while ((IoTHubClient_LL_GetSendStatus(_iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY));

    return ESP_OK;
}
