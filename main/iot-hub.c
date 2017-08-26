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

#include "device-config.h"
#include "iot-hub.h"
#include "telemetry-data.h"

typedef struct 
{
    const char * hostname;
    const char * device_id;
    const char * primary_key;
    QueueHandle_t telemetry_queue;
} hub_configuration_t;

typedef struct
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;  // For tracking the messages within the user callback.
} EVENT_INSTANCE;

static const char *TAG = "iot-hub";
static hub_configuration_t _config;
IOTHUB_CLIENT_LL_HANDLE _iotHubClientHandle = NULL;

void ConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void * ptr)
{
    if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
        ESP_LOGI(TAG, "Connected to IoT Hub\n");
        xEventGroupSetBits(_wifi_event_group, IOTHUB_CONNECTED_BIT);
    }
    else if (reason == IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN) {
        ESP_LOGE(TAG, "Disconnected from IoT Hub: Expired shared access token");
        xEventGroupClearBits(_wifi_event_group, IOTHUB_INITIALIZED_BIT);
        xEventGroupClearBits(_wifi_event_group, IOTHUB_CONNECTED_BIT);
    }
    else {
        ESP_LOGE(TAG, "Disconnected from IoT Hub with reason=%d\n", reason);
        xEventGroupClearBits(_wifi_event_group, IOTHUB_CONNECTED_BIT);
    }
}

void ReportedStateCallback(int status_code, void * userContextCallback)
{
    ESP_LOGI(TAG, "Reported State Callback with status code %d", status_code)
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

void DeviceTwinUpdateStateCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payLoad, size_t size, void* userContextCallback)
{
    char * buf = malloc(size + 1);
    memcpy(buf, payLoad, size);
    buf[size] = 0;
    
    if (update_state == DEVICE_TWIN_UPDATE_COMPLETE)
    {
        ESP_LOGI(TAG, "Complete Update request received\n");
    }
    else
    {
        ESP_LOGI(TAG, "Partial Update request received\n");
    }

    ESP_LOGI(TAG, "Payload: %s\n", buf);

    free(buf);
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
        ESP_LOGE(TAG, "Failed to retrieve the message data\n");
    }
    else
    {
        ESP_LOGI(TAG, "Received Message [%d]\n Message ID: %s\n Correlation ID: %s\n Data: <<<%.*s>>> & Size=%d\n", 
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

                ESP_LOGI(TAG, " Message Properties:\n");

                for (index = 0; index < propertyCount; index++)
                {
                    ESP_LOGI(TAG, "\tKey: %s Value: %s\n", keys[index], values[index]);
                }
                ESP_LOGI(TAG, "\n");
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

    ESP_LOGI(TAG, "%s", responseString);
    
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

    ESP_LOGI(TAG, "Confirmation received for message tracking id = %zu with result = %s\n", 
        eventInstance->messageTrackingId, 
        ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

    /* Some device specific action code goes here... */

    IoTHubMessage_Destroy(eventInstance->messageHandle);
}

esp_err_t dispatch_telemetry_data(telemetry_message_handle_t telemetry_message)
{
    static EVENT_INSTANCE message;
    static int messageCounter;

    char * json = telemetry_message_to_json(telemetry_message);

    message.messageTrackingId = ++messageCounter;
    message.messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)json, strlen(json));

    if (message.messageHandle == NULL)
    {
        ESP_LOGE(TAG, "IoT Message creation failed\n");
        return ESP_FAIL;
    }

    IoTHubMessage_SetMessageId(message.messageHandle, "MSG_ID");
    IoTHubMessage_SetCorrelationId(message.messageHandle, "CORE_ID");

    /*
    MAP_HANDLE propMap = IoTHubMessage_Properties(message.messageHandle);
    if (Map_AddOrUpdate(propMap, "temperatureAlert", temperature > 28 ? "true" : "false") != MAP_OK)
    {
        ESP_LOGE(TAG, "Failed to create temperature alert property");
    }
*/
    ESP_LOGI(TAG, "Sending Message: %s", json);

    telemetry_message_dispose_json(json);
    telemetry_message_destroy(telemetry_message);

    if (IoTHubClient_LL_SendEventAsync(_iotHubClientHandle, message.messageHandle, SendConfirmationCallback, &message) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(TAG, "IoTHubClient_LL_SendEventAsync Failed\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "IoTHubClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\n", messageCounter);

    return ESP_OK;
}

esp_err_t iothub_connect()
{
    ESP_LOGI(TAG, "Connecting to Hub");
    static int receiveContext;
    
    char connectionString[256];
    
    if (sprintf_s(connectionString, sizeof(connectionString), "HostName=%s;DeviceId=%s;SharedAccessKey=%s",
        _config.hostname, _config.device_id, _config.primary_key) <= 0) 
    {
        ESP_LOGE(TAG, "Failed to create the connection string\n");
        return ESP_FAIL;
    }
        
    if (platform_init() != 0)
    {
        ESP_LOGE(TAG, "Failed to initialize the platform\n");
        return ESP_FAIL;
    }

    _iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);    

    if (_iotHubClientHandle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create the IoT Hub connection\n");
        platform_deinit();
        return ESP_FAIL;
    }

    bool traceOn = true;
    IoTHubClient_LL_SetOption(_iotHubClientHandle, "logtrace", &traceOn);

    if (IoTHubClient_LL_SetConnectionStatusCallback(_iotHubClientHandle, ConnectionStatusCallback, NULL) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(TAG, "Failed to register connection status callback function\n");
    }

    /* Setting Message call back, so we can receive Commands. */
    if (IoTHubClient_LL_SetMessageCallback(_iotHubClientHandle, ReceiveMessageCallback, &receiveContext) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(TAG, "Failed to register message callback function\n");
    }

    /* Register Device Method */
    if (IoTHubClient_LL_SetDeviceMethodCallback(_iotHubClientHandle, DeviceMethodCallback, NULL) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(TAG, "Failed to register device method callback function");
    }

    /* Register Device Twin Callback method */
    if (IoTHubClient_LL_SetDeviceTwinCallback(_iotHubClientHandle, DeviceTwinUpdateStateCallback, NULL) != IOTHUB_CLIENT_OK)
    {
        ESP_LOGE(TAG, "Failed to register device twin callback function");
    }

    return ESP_OK;
}

void iothub_disconnect()
{
    if (_iotHubClientHandle != NULL)
    {
        IoTHubClient_LL_Destroy(_iotHubClientHandle);
        (void)printf("Handle Destroyed\r\n");
        
        _iotHubClientHandle = NULL;
        
        platform_deinit();
        (void)printf("Platform de-initiazed\r\n");
    }
    
}

void task_process_sensor_telemetry(void * ptr)
{
    telemetry_message_handle_t message;
    IOTHUB_CLIENT_STATUS status;

    while(true)
    {
        xEventGroupWaitBits(_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
        
        // Reconnect to hub everytime we are disonnected
        if ((xEventGroupGetBits(_wifi_event_group) & IOTHUB_INITIALIZED_BIT) == 0) 
        {
            if (iothub_connect() == ESP_OK)
            {
                ESP_LOGI(TAG, "IoT Hub Initialized");
                xEventGroupSetBits(_wifi_event_group, IOTHUB_INITIALIZED_BIT);
            }
        } 
        // Process data from the telemetry queue
        else if (xQueueReceive(_config.telemetry_queue, &message, _device_configuration.hub_pooling_rate / portTICK_PERIOD_MS)) 
        {
            dispatch_telemetry_data(message);
        } 
        else 
        {
            // Process events from the hub queue
            do    
            {
                IoTHubClient_LL_DoWork(_iotHubClientHandle);
            }
            while ((IoTHubClient_LL_GetSendStatus(_iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY));
        }

        if ((xEventGroupGetBits(_wifi_event_group) & IOTHUB_INITIALIZED_BIT) == 0)
        {
            ESP_LOGI(TAG, "Disconected from hub");
            iothub_disconnect();
            vTaskDelay(_device_configuration.hub_pooling_rate / portTICK_PERIOD_MS);
        }
    }    
}

esp_err_t iothub_init(const char * hostname, const char * device_id, const char * primary_key, const QueueHandle_t telemetry_queue)
{
    _config.hostname = hostname;
    _config.device_id = device_id;
    _config.primary_key = primary_key;
    _config.telemetry_queue = telemetry_queue;

    xTaskCreate(task_process_sensor_telemetry, "IoT Hub Thread", 8192, (void *) telemetry_queue, 5, NULL);

    return ESP_OK;
}
