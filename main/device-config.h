#ifndef __DEVICE_CONFIG_H__
#define __DEVICE_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* Sensor configuration from menu-config */
#define I2C_SCL_IO                   CONFIG_I2C_SCL_IO
#define I2C_SDA_IO                   CONFIG_I2C_SDA_IO
#define I2C_PORT                     I2C_NUM_1
#define I2C_FREQ_HZ                  100000     /*!< I2C master clock frequency */
#define MCP9808_SENSOR_ADDR          CONFIG_MCP9808_SENSOR_ADDR

/* IoT configuration from menu-config */
#define HUB_WIFI_SSID                 CONFIG_WIFI_SSID
#define HUB_WIFI_PASS                 CONFIG_WIFI_PASSWORD
#define HUB_AZURE_HOST_NAME           CONFIG_AZURE_HOST_NAME
#define HUB_AZURE_DEVICE_ID           CONFIG_AZURE_DEVICE_ID
#define HUB_AZURE_DEVICE_PRIMARY_KEY  CONFIG_AZURE_DEVICE_PRIMARY_KEY

/* Thread initialization synchronization bits for RTOS event groups */
#define WIFI_CONNECTED_BIT            BIT0
#define IOTHUB_INITIALIZED_BIT        BIT1
#define IOTHUB_CONNECTED_BIT          BIT2

/*
 * The device configuration type
 */
typedef struct {
    /*
    * Telemetry sampling rate. Number of ms between each sesnsor reading.
    * Range: 30000 - 300000 (30 sec. - 5 minutes)
    * Default: 60000 (1 minute)
    */
    uint16_t sensor_sampling_rate;

    /*
    * The IoT hub message queue pooling rate. Number of ms between each time
    * the IoT message queue is pooled for messages
    * Range: 1 - 1000 (1ms - 1 sec.)
    * Default: 100 (10 times per second)
    */
    uint16_t hub_pooling_rate;
} device_config_t;

/* 
 * FreeRTOS event group to synchronize thread initialization:
 *   - IoT Hub thread waits for Wi-Fi to be connected
 *   - IoT Hub event pump thread waits or IoT Hub to be initialized
 *   - Sensor thread waits for IoT Hub to be connected
 */
EventGroupHandle_t _wifi_event_group;

/*
 * Device configuration set by the IoT Hub through device twin messaging.
 */
device_config_t _device_configuration;

#ifdef __cplusplus
}
#endif

#endif
