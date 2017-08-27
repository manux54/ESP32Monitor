#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "device-config.h"
#include "dht11.h"
#include "telemetry-data.h"

#include "driver/gpio.h"

static uint8_t _dht11_pin;
static QueueHandle_t _telemetry_queue;

static int8_t _temperature;
static int8_t _humidity;

static const char *TAG = "DHT11";

PRIVILEGED_DATA portMUX_TYPE _readerMux = portMUX_INITIALIZER_UNLOCKED;

int32_t dht11_readPulse(int32_t usTimeOut, bool state )
{
    int32_t usTime = 0;

    while( gpio_get_level(_dht11_pin) == state )
    {
        if( usTime++ > usTimeOut )
        {
            return -1;
        }

		ets_delay_us(1);		// uSec delay
	}
	
	return usTime;
}

esp_err_t dht11_read()
{
    uint8_t dht_data[5];       // read 5 bytes (40 bits)
    uint8_t byte_index = 0;    
    uint8_t bit_index = 7;     // start with most significant bit

    memset(dht_data, 0, sizeof(dht_data));

    taskENTER_CRITICAL(&_readerMux);

    // send start signal
    gpio_set_direction(_dht11_pin, GPIO_MODE_OUTPUT);
    
    // Set the line low for 20ms
    gpio_set_level(_dht11_pin, 0);
    ets_delay_us(22000);

    // End the start signal by setting the data line to high for 40us 
    gpio_set_level(_dht11_pin, 1);
    ets_delay_us(43);

    // Start reading the data.
    gpio_set_direction(_dht11_pin, GPIO_MODE_INPUT);

    // DHT11 sensor will keep the line low for 80us.
    int32_t pulse = dht11_readPulse(85, 0);

    if (pulse < 0)
    {
        ESP_LOGE(TAG, "Expected low signal for 80us");
        taskEXIT_CRITICAL(&_readerMux);
        return ESP_FAIL;
    }
    else if (pulse == 0)
    {
        ESP_LOGE(TAG, "Expected low signal, but signal was high");
        taskEXIT_CRITICAL(&_readerMux);
        return ESP_FAIL;
    }

    // Then DHT11 sensor will keep the line high for 80us
    pulse = dht11_readPulse(85, 1);

    if (pulse < 0)
    {
        ESP_LOGE(TAG, "Expected high signal for 80us");
        taskEXIT_CRITICAL(&_readerMux);
        return ESP_FAIL;
    }
    else if (pulse == 0)
    {
        ESP_LOGE(TAG, "Expected high signal, but signal was low");
        taskEXIT_CRITICAL(&_readerMux);
        return ESP_FAIL;
    }

    // No errors, read the 40 bits of data
    for (uint8_t bit = 0; bit < 40; ++bit)
    {
        // Each bits are 50us low followed by either 28us high for 0 or 70us high for 1
        pulse = dht11_readPulse(60, 0);

        if (pulse <= 0)
        {
            ESP_LOGE(TAG, "Expected low signal for 40us");
            taskEXIT_CRITICAL(&_readerMux);
            return ESP_FAIL;
        }

        pulse = dht11_readPulse(75, 1);

        if (pulse <= 0)
        {
            ESP_LOGE(TAG, "Timed out reading data signal");
            taskEXIT_CRITICAL(&_readerMux);
            return ESP_FAIL;
        }

        // 1 if pulse is closer to 70us
        if (pulse > 40)
        {
            dht_data[byte_index] |= (1 << bit_index);
        }

        if (bit_index == 0)
        {
            // read next byte
            ++byte_index;
            bit_index = 7;
        }
        else
        {
            // read next bit
            --bit_index;
        }
    }

    taskEXIT_CRITICAL(&_readerMux);
    
    _temperature = dht_data[2];
    _humidity = dht_data[0];

    return ESP_OK;
}

void task_poll_dht11_sensor_telemetry(void * ptr)
{
    ESP_LOGI(TAG, "Waiting or IoT Hub Connection");

    // Wait until IoT Hub is connected
    xEventGroupWaitBits(_wifi_event_group, IOTHUB_CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Hub Connected. Starting telemetry readings");

	vPortCPUInitializeMutex( &_readerMux );
    
    while(true)
    {
        // Read temperature
        if( dht11_read() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read temperature from sensor\n");
        }
        else
        {
            telemetry_message_handle_t message = telemetry_message_create_new();

            telemetry_message_add_number(message, "temperature", (double)_temperature);
            telemetry_message_add_number(message, "humidity", (double)_humidity);
    
            // Send temperature on telemetry queue
            if (!xQueueSend(_telemetry_queue, &message, 500))
            {
                ESP_LOGE(TAG, "Failed to send temperaturmessagee to queue within 500ms\n");
                telemetry_message_destroy(message);
            }
        }

        // Wait for current sampling delay
        vTaskDelay(_device_configuration.sensor_sampling_rate/portTICK_PERIOD_MS);
    }
}


/**
 * @brief Initialize communication with the dht11 sensor on the specified pin. A task will be created and
 * sensor telemetry (temperature and humidity) data will be posted on the queue.
 * 
 * @param[in]  pin              The DHT11 data pin
 * @param[in]  telemetry_queue  The queue's handle on which telemetry data will be posted.
 * 
 * @return 
 *          - ESP_OK if the DHT11 device was successfully initialized
 *          - ESP_FAIL otherwise  
 */
esp_err_t dht11_init(uint8_t pin, QueueHandle_t telemetry_queue)
{
    _dht11_pin = pin;
    _telemetry_queue = telemetry_queue;

    xTaskCreate(task_poll_dht11_sensor_telemetry, "DHT 11 Sensor Polling Thread", 2048, NULL, 5, NULL);
    
    return ESP_OK;
}
