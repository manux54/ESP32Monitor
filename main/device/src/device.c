#include "device.h"
#include "device-config.h"

#include "esp_log.h"

#include <string.h>

typedef struct SENSOR_QUEUE_TAG
{
    SENSOR_HANDLE handle;
    const SENSOR_INTERFACE_DESCRIPTION * interface;
    struct SENSOR_QUEUE_TAG * next;
} SENSOR_QUEUE;

typedef struct DEVICE_TAG
{
    char * deviceId;
    QueueHandle_t telemetry_queue;
    SENSOR_QUEUE * sensors;
} DEVICE;

static const char *TAG = "DEVICE";

void task_poll_sensors_telemetry(void * ptr);

/**
 * @brief Create a device. Each device create a thread, read sensors and dispatch sensor telemetry to the iot-hub
 *        through a messaging queue.
 * 
 * @param[in]  deviceId         The device's name
 * @param[in]  telemetry_queue  The queue's handle on which telemetry data will be posted.
 * 
 * @return 
 *          - The device's handle  
 */
DEVICE_HANDLE device_create(const char * deviceId, QueueHandle_t telemetry_queue)
{
    DEVICE * device = malloc(sizeof(DEVICE));
    device->deviceId = malloc(strlen(deviceId) + 1);
    strcpy(device->deviceId, deviceId);
    device->sensors = NULL;
    device->telemetry_queue = telemetry_queue;

    return (DEVICE_HANDLE) device;
}

/**
 * @brief Stops the device and dispose of    allocated resources
 *
 * @param[in]  handle        The device for which to dispose the resources
 *
 */
void device_destroy(DEVICE_HANDLE handle)
{
    DEVICE * device = (DEVICE *) handle;

    if (device != NULL)
    {
        if (device->deviceId != NULL)
        {
            free(device->deviceId);
        }

        while (device->sensors != NULL)
        {
            SENSOR_QUEUE * sensor = device->sensors;
            device->sensors = sensor->next;
            sensor->interface->sensor_destroy(sensor->handle);
            free(sensor);
        }

        free(device);
    }
}

/**
 * @brief Add a sensor to this device. Each sensor's add its telemetry to the messaging
 *
 * @param[in]  handle             The device's handle from device_create
 * @param[in]  sensor_interface   The sensor's interface
 * @param[in]  sensor_options     The sensor's options to be used in SENSOR_SET_OPTIONS
 *
 * @return
 *          - DEVICE_STATUS_OK if sensor added successfully
 *          - DEVICE_STATUS_FAILED if unable to add sensor
 */
uint32_t device_add_sensor(DEVICE_HANDLE handle, const SENSOR_INTERFACE_DESCRIPTION * sensor_interface, void * sensor_options)
{
    DEVICE * device = (DEVICE *) handle;
    
    if (device != NULL)
    {
        SENSOR_QUEUE * sensor = malloc(sizeof(SENSOR_QUEUE));

        if (sensor != NULL)
        {
            sensor->handle = sensor_interface->sensor_create();
            sensor_interface->sensor_set_options(sensor->handle, sensor_options);
            sensor->interface = sensor_interface;
            sensor->next = device->sensors;
            device->sensors = sensor;

            return DEVICE_STATUS_OK;
        }
    }

    return DEVICE_STATUS_FAILED;
}
 
/**
 * @brief  Starts reading telemetry from the sensor and send data to the messaging queue.
 *
 * @param[in]  handle          The device's handle from device_create
 * 
 * @return
 *          - DEVICE_STATUS_OK if device started successfully
 *          - DEVICE_STATUS_FAILED if unable to start device
 */
uint32_t device_start(DEVICE_HANDLE handle)
{
    DEVICE * device = (DEVICE *) handle;

    if (device != NULL)
    {
        SENSOR_QUEUE * sensor = device->sensors;

        while (sensor != NULL)
        {
            if (sensor->interface->sensor_initialize(sensor->handle) != SENSOR_STATUS_OK)
            {
                ESP_LOGI(TAG, "Failed to initialize sensor\n");
                return DEVICE_STATUS_FAILED;
            }

            sensor = sensor->next;
        }

        xTaskCreate(task_poll_sensors_telemetry, "Sensors Polling Thread", 2048, (void *) device, 5, NULL);
        return DEVICE_STATUS_OK;
    }

    return DEVICE_STATUS_FAILED;
}

void task_poll_sensors_telemetry(void * ptr)
{
    DEVICE * device = (DEVICE *) ptr;

    ESP_LOGI(TAG, "Waiting or IoT Hub Connection");

    // Wait until IoT Hub is connected
    xEventGroupWaitBits(_wifi_event_group, IOTHUB_CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Hub Connected. Starting telemetry readings");

    while(true)
    {
        telemetry_message_handle_t message = telemetry_message_create_new();
        telemetry_message_add_string(message, "deviceId", device->deviceId);
        
        SENSOR_QUEUE * sensor = device->sensors;

        while (sensor != NULL)
        {
            if (sensor->interface->sensor_read(sensor->handle) == SENSOR_STATUS_OK)
            {
                sensor->interface->sensor_post_results(sensor->handle, message);
            }

            sensor = sensor->next;
        }
        
        // Send temperature on telemetry queue
        if (!xQueueSend(device->telemetry_queue, &message, 500))
        {
            ESP_LOGE(TAG, "Failed to send telemetry to queue within 500ms\n");
            telemetry_message_destroy(message);
        }

        // Wait for current sampling delay
        vTaskDelay(_device_configuration.sensor_sampling_rate/portTICK_PERIOD_MS);
    }
}
