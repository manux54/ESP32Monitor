#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "esp_wifi.h"
#include "esp_event_loop.h" 
#include "esp_log.h"

#include "nvs_flash.h"

#include "device-config.h"
#include "mcp9808.h"
#include "iot-hub.h"
#include "telemetry-data.h"

/* Logging Tag */
static const char *TAG = "main-hub";

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
            xEventGroupSetBits(_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected");
            xEventGroupClearBits(_wifi_event_group, WIFI_CONNECTED_BIT);
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
    _wifi_event_group = xEventGroupCreate();

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
    
    // TODO: Load configuration from the WiFi NVS flash if possible
    // TODO: Set storage to flash?
    // TODO: esp_wifi_get_config...
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

esp_err_t initialize_i2c() {

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        
        .sda_io_num = I2C_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,

        .scl_io_num = I2C_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,

        .master.clk_speed = I2C_FREQ_HZ
    };

    i2c_param_config(I2C_PORT, &config);

    return i2c_driver_install(I2C_PORT, config.mode, 0, 0, 0);
}

void app_main()
{
    // Initialize default configuration
    _device_configuration.sensor_sampling_rate = 30000;
    _device_configuration.hub_pooling_rate = 500;

    // Initialize the telemetry queue
    QueueHandle_t telemetry_queue = xQueueCreate(1, sizeof(telemetry_message_handle_t));
    
    // Initialize WiFi
    nvs_flash_init();
    initialize_wifi();

    // Initialize i2c Driver
    initialize_i2c();

    // Initialize Pins
    gpio_pad_select_gpio(2);
    gpio_set_direction(2, GPIO_MODE_OUTPUT);

    // Initialize threads
    iothub_init(HUB_AZURE_HOST_NAME, HUB_AZURE_DEVICE_ID, HUB_AZURE_DEVICE_PRIMARY_KEY, telemetry_queue);
    mcp9808_init(I2C_PORT, MCP9808_SENSOR_ADDR, telemetry_queue);
}
