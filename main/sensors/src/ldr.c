#include "ldr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

#include "driver/adc.h"

#include <string.h>

SENSOR_HANDLE ldr_create();
void ldr_destroy(SENSOR_HANDLE handle);
void ldr_set_options(SENSOR_HANDLE handle, void * options);
void* ldr_get_options(SENSOR_HANDLE handle);
int ldr_initialize(SENSOR_HANDLE handle);
int ldr_read(SENSOR_HANDLE handle);
int ldr_post(SENSOR_HANDLE handle, telemetry_message_handle_t message);

static const SENSOR_INTERFACE_DESCRIPTION ldr_handle_interface_description =
{
    ldr_create,
    ldr_destroy,
    ldr_set_options,
    ldr_get_options,
    ldr_initialize,
    ldr_read,
    ldr_post
};

typedef enum
{
    LDR_SENSOR_STATUS_CREATED,
    LDR_SENSOR_STATUS_READY,
    LDR_SENSOR_STATUS_INVALID_TELEMETRY
} LDR_SENSOR_STATUS;

typedef struct LDR_SENSOR_TAG
{
    LDR_SENSOR_OPTIONS * options;
    double voltage;
    double lightResistance;
    LDR_SENSOR_STATUS status;
} LDR_SENSOR;

static const char *TAG = "LDR Sensor";

const SENSOR_INTERFACE_DESCRIPTION * ldr_get_inteface()
{
    return &ldr_handle_interface_description;
}

SENSOR_HANDLE ldr_create()
{
    LDR_SENSOR * sensor = malloc(sizeof(LDR_SENSOR));
    sensor->options = malloc(sizeof(LDR_SENSOR_OPTIONS));
    sensor->voltage = 0;
    sensor->lightResistance = 0;
    sensor->status = LDR_SENSOR_STATUS_CREATED;

    return (SENSOR_HANDLE) sensor;
}

void ldr_destroy(SENSOR_HANDLE handle)
{
    LDR_SENSOR * sensor = (LDR_SENSOR *) handle;

    if (sensor != NULL)
    {
        if(sensor->options != NULL)
        {
            free(sensor->options);
        }

        free(sensor);
    }
}

void ldr_set_options(SENSOR_HANDLE handle, void * options)
{
    LDR_SENSOR * sensor = (LDR_SENSOR *) handle;
    LDR_SENSOR_OPTIONS * opt = (LDR_SENSOR_OPTIONS *) options; 

    if (sensor != NULL)
    {
        sensor->options->pin = opt->pin;
    }
}

void* ldr_get_options(SENSOR_HANDLE handle)
{
    LDR_SENSOR * sensor = (LDR_SENSOR *) handle;
    return (void *) sensor->options;
}

int ldr_initialize(SENSOR_HANDLE handle)
{
    LDR_SENSOR * sensor = (LDR_SENSOR *) handle;

    adc1_channel_t channel;

    switch(sensor->options->pin)
    {
        case 32: channel = ADC1_CHANNEL_4; break;
        case 33: channel = ADC1_CHANNEL_5; break;
        case 34: channel = ADC1_CHANNEL_6; break;
        case 35: channel = ADC1_CHANNEL_7; break;
        case 36: channel = ADC1_CHANNEL_0; break;
        case 37: channel = ADC1_CHANNEL_1; break;
        case 38: channel = ADC1_CHANNEL_2; break;
        case 39: channel = ADC1_CHANNEL_3; break;
        default:
            ESP_LOGE(TAG, "Invalid Configuration, Expected pin 32-39");
            return SENSOR_STATUS_FAILED;
    }

    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(channel, ADC_ATTEN_11db);
    
    return SENSOR_STATUS_OK;
}

int ldr_read(SENSOR_HANDLE handle)
{
    LDR_SENSOR * sensor = (LDR_SENSOR *) handle;
    sensor->status = LDR_SENSOR_STATUS_READY;

    int reading = adc1_get_voltage(ADC1_CHANNEL_4);

    sensor->voltage = (double) reading / 4095.0 * 3.3;
    sensor->lightResistance = 10.0 * (3.3 - sensor->voltage) / 3.3;

    ESP_LOGI(TAG, "Reading = %d; Voltage = %f; Resisance = %fk\n", reading, sensor->voltage, sensor->lightResistance);
    
    return SENSOR_STATUS_OK;
}

int ldr_post(SENSOR_HANDLE handle, telemetry_message_handle_t message)
{
    LDR_SENSOR * sensor = (LDR_SENSOR *) handle;
    
    if (sensor->status == LDR_SENSOR_STATUS_READY)
    {
        telemetry_message_add_number(message, "ldrVoltage", sensor->voltage);
        telemetry_message_add_number(message, "ldrResistance", sensor->lightResistance);
        return SENSOR_STATUS_OK;
    }

    return SENSOR_STATUS_FAILED;
}
