#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "mcp9808.h"

#define I2C_SCL_IO    CONFIG_I2C_SCL_IO
#define I2C_SDA_IO    CONFIG_I2C_SDA_IO
#define MCP9808_SENSOR_ADDR    CONFIG_MCP9808_SENSOR_ADDR

#define SENSOR_RATE 5000

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

void app_main()
{
    QueueHandle_t queue = xQueueCreate(1, sizeof(float));

    xTaskCreate(task_censor_monitor, "Temperature Sensor Monitor", 2048, (void *) queue, 5, NULL);
    xTaskCreate(task_process_monitor_queue, "Temperature Sensor Monitor", 2048, (void *) queue, 5, NULL);
}
