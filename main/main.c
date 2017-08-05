#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event_loop.h" 
#include "esp_log.h"

#include "mcp9808.h"

#define I2C_SCL_IO                   CONFIG_I2C_SCL_IO
#define I2C_SDA_IO                   CONFIG_I2C_SDA_IO
#define MCP9808_SENSOR_ADDR          CONFIG_MCP9808_SENSOR_ADDR

#define HUB_WIFI_SSID                 CONFIG_WIFI_SSID
#define HUB_WIFI_PASS                 CONFIG_WIFI_PASSWORD
#define HUB_AZURE_HOST_NAME           CONFIG_AZURE_HOST_NAME
#define HUB_AZURE_DEVICE_ID           CONFIG_AZURE_DEVICE_ID
#define HUB_AZURE_DEVICE_PRIMARY_KEY  CONFIG_AZURE_DEVICE_PRIMARY_KEY

#define SENSOR_RATE 5000

/* Logging Tag */
static const char *TAG = "main-hub";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 - are we connected to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

void process_sensor_data(float temperature)
{
    printf("Temperature = %.4f C\n", temperature);
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

        vTaskDelay(SENSOR_RATE/portTICK_PERIOD_MS);
    }
}

void task_process_monitor_queue(void * ptr)
{
    QueueHandle_t queue = (QueueHandle_t)ptr;

    ESP_LOGI(TAG, "Waiting for WiFi access point ...");

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to access point success");

    float temperature;

    while(true)
    {
        if (!xQueueReceive(queue, &temperature, SENSOR_RATE))
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
    initialize_wifi();

    xTaskCreate(task_censor_monitor, "Temperature Sensor Monitor", 2048, (void *) queue, 5, NULL);
    xTaskCreate(task_process_monitor_queue, "Temperature Sensor Monitor", 2048, (void *) queue, 5, NULL);
}
