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

#include "iot-hub.h"

/* Logging Tag */
typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;  // For tracking the messages within the user callback.
} EVENT_INSTANCE;

static const char *IOT_TAG = "iot-hub";
static const char *_deviceId;

static int _callbackCounter;
static int _messageCounter;

IOTHUB_CLIENT_LL_HANDLE _iotHubClientHandle = NULL;
EVENT_INSTANCE _message;

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

void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    ESP_LOGI(IOT_TAG, "Confirmation Received");

    EVENT_INSTANCE* eventInstance = (EVENT_INSTANCE*)userContextCallback;
    ESP_LOGI(IOT_TAG, "Confirmation[%d] received for message tracking id = %zu with result = %s\n", 
        _callbackCounter, 
        eventInstance->messageTrackingId, 
        ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

    /* Some device specific action code goes here... */
    _callbackCounter++;
    IoTHubMessage_Destroy(eventInstance->messageHandle);
}

esp_err_t iothub_init(const char * hostname, const char * deviceId, const char * primaryKey)
{
    _callbackCounter = 0;
    _messageCounter = 0;
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
    
    return ESP_OK;
}

esp_err_t iothub_processCensorData(const float temperature)
{
    char msgText[100];
    sprintf(msgText, "{\"deviceId\":\"%s\",\"temperature\":%.4f}", _deviceId, temperature);

    _message.messageTrackingId = ++_messageCounter;
    _message.messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText));

    if (_message.messageHandle == NULL)
    {
        ESP_LOGE(IOT_TAG, "IoT Message creation failed\n");
        return ESP_FAIL;
    }

    IoTHubMessage_SetMessageId(_message.messageHandle, "MSG_ID");
    IoTHubMessage_SetCorrelationId(_message.messageHandle, "CORE_ID");

    MAP_HANDLE propMap = IoTHubMessage_Properties(_message.messageHandle);
    if (Map_AddOrUpdate(propMap, "temperatureAlert", temperature > 28 ? "true" : "false") != MAP_OK)
    {
        ESP_LOGE(IOT_TAG, "Failed to create temperature alert property");
    }

    if (IoTHubClient_LL_SendEventAsync(_iotHubClientHandle, _message.messageHandle, SendConfirmationCallback, &_message) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(IOT_TAG, "IoTHubClient_LL_SendEventAsync Failed\n");
        return ESP_FAIL;
    }

    ESP_LOGI(IOT_TAG, "IoTHubClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\n", (int)_messageCounter);
    ESP_LOGI(IOT_TAG, "Message: %s", msgText);

    return ESP_OK;
}

esp_err_t iothub_processEventQueue()
{
    IOTHUB_CLIENT_STATUS status;
    
    while ((IoTHubClient_LL_GetSendStatus(_iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY))
    {
        IoTHubClient_LL_DoWork(_iotHubClientHandle);
    }

    return ESP_OK;
}
