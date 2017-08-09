#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"

#include "esp_wifi.h"
#include "esp_event_loop.h" 
#include "esp_log.h"

#include "nvs_flash.h"

#include "mcp9808.h"
#include "iot-hub.h"

#define I2C_SCL_IO                   CONFIG_I2C_SCL_IO
#define I2C_SDA_IO                   CONFIG_I2C_SDA_IO
#define MCP9808_SENSOR_ADDR          CONFIG_MCP9808_SENSOR_ADDR

#define HUB_WIFI_SSID                 CONFIG_WIFI_SSID
#define HUB_WIFI_PASS                 CONFIG_WIFI_PASSWORD
#define HUB_AZURE_HOST_NAME           CONFIG_AZURE_HOST_NAME
#define HUB_AZURE_DEVICE_ID           CONFIG_AZURE_DEVICE_ID
#define HUB_AZURE_DEVICE_PRIMARY_KEY  CONFIG_AZURE_DEVICE_PRIMARY_KEY

/* Logging Tag */
static const char *TAG = "main-hub";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

const int CONNECTED_BIT = BIT0;
const int AZURE_INIT_BIT = BIT1;

unsigned int _refreshRate = 0;

void process_sensor_data(float temperature)
{
    printf("Temperature = %.4f C\n", temperature);
    iothub_processCensorData(temperature);
}

void task_censor_monitor(void * ptr)
{
    QueueHandle_t queue = (QueueHandle_t)ptr;
    
    mcp9808_handle handle;
    esp_err_t status = mcp9808_init(MCP9808_SENSOR_ADDR, I2C_SDA_IO, I2C_SCL_IO, &handle);
    
    if (status != ESP_OK) {
        puts("Failed to initialize MCP9808 sensor\n");
        while(true) { vTaskDelay(1000); }
    }

    xEventGroupWaitBits(wifi_event_group, AZURE_INIT_BIT, false, true, portMAX_DELAY);

    float temperature;

    while(true)
    {
        status = mcp9808_read_temp_c(handle, &temperature);

        if (status != ESP_OK) {
            puts("Failed to read temperature from sensor\n");
        }

        if (!xQueueSend(queue, &temperature, 500))
        {
            puts("Failed to send temperature to queue within 500ms\n");
        }

        if (_refreshRate != iothub_refreshRate())
        {
            _refreshRate = iothub_refreshRate();
            iothub_reportTwinData(_refreshRate);
        }

        vTaskDelay(_refreshRate/portTICK_PERIOD_MS);
    }
}

void task_process_monitor_queue(void * ptr)
{
    QueueHandle_t queue = (QueueHandle_t)ptr;

    ESP_LOGI(TAG, "Waiting for WiFi access point ...");

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to access point success");

    if (iothub_init(HUB_AZURE_HOST_NAME, HUB_AZURE_DEVICE_ID, HUB_AZURE_DEVICE_PRIMARY_KEY) == ESP_OK)
    {
        ESP_LOGI(TAG, "IoT Hub Initialized");
    }

    xEventGroupSetBits(wifi_event_group, AZURE_INIT_BIT);

    float temperature;

    while(true)
    {
        if (!xQueueReceive(queue, &temperature, _refreshRate))
        {
            puts("Failed to receive sensor data");
        }
        else
        {
            process_sensor_data(temperature);
        }
    }    
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi started");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "WiFi connected");
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            esp_wifi_connect();
            break;
        default:
            break;
    }

    return ESP_OK;
}

void task_process_iot_message_queue(void * ptr)
{
    ESP_LOGI(TAG, "Waiting for WiFi access point ...");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Waiting for azure messages ...");

    while(true)
    {
        // Process events off the queue, at least twice a second.
        iothub_processEventQueue();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
}

esp_err_t initialize_wifi()
{
    // Create the event group for wifi connection status notifications
    wifi_event_group = xEventGroupCreate();

    // Initialize the wifi. 
    // Ref: http://esp-idf.readthedocs.io/en/latest/api-guides/wifi.html#esp32-wi-fi-station-general-scenario
    tcpip_adapter_init();

    // Initialize an event callback function to process wifi lifecycle events
    esp_err_t status = esp_event_loop_init(event_handler, NULL);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing wifi event loop");
        return status;
    }

    // Initialize and configure the wifi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    status = esp_wifi_init(&cfg);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing wifi driver");
        return status;
    }
    
    status = esp_wifi_set_storage(WIFI_STORAGE_RAM);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing wifi storage");
        return status;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = HUB_WIFI_SSID,
            .password = HUB_WIFI_PASS,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);

    status = esp_wifi_set_mode(WIFI_MODE_STA);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Error settig wifi mode to station");
        return status;
    }

    status = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Error configuring wifi");
        return status;
    }

    status = esp_wifi_start();

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Error starting wifi");
        return status;
    }

    return status;
}

void app_main()
{
    QueueHandle_t queue = xQueueCreate(1, sizeof(float));
    
    nvs_flash_init();
    initialize_wifi();

    gpio_pad_select_gpio(2);
    gpio_set_direction(2, GPIO_MODE_OUTPUT);

    xTaskCreate(task_censor_monitor, "Temperature Sensor Monitor", 2048, (void *) queue, 5, NULL);
    xTaskCreate(task_process_monitor_queue, "Temperature Sensor Monitor", 8192, (void *) queue, 5, NULL);
    xTaskCreate(task_process_iot_message_queue, "IoT message dispatcher", 8192, NULL, 5, NULL);
}
