#include "dht.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include <string.h>

SENSOR_HANDLE dht_create();
void dht_destroy(SENSOR_HANDLE handle);
void dht_set_options(SENSOR_HANDLE handle, void * options);
void* dht_get_options(SENSOR_HANDLE handle);
int dht_initialize(SENSOR_HANDLE handle);
int dht_read(SENSOR_HANDLE handle);
int dht_post(SENSOR_HANDLE handle, telemetry_message_handle_t message);

static const SENSOR_INTERFACE_DESCRIPTION dht_handle_interface_description =
{
    dht_create,
    dht_destroy,
    dht_set_options,
    dht_get_options,
    dht_initialize,
    dht_read,
    dht_post
};

typedef enum
{
    DHT_SENSOR_STATUS_CREATED,
    DHT_SENSOR_STATUS_READY,
    DHT_SENSOR_STATUS_INVALID_TELEMETRY
} DHT_SENSOR_STATUS;

typedef struct DHT_SENSOR_TAG
{
    DHT_SENSOR_OPTIONS * options;
    double temperature;
    double humidity;
    DHT_SENSOR_STATUS status;
} DHT_SENSOR;

PRIVILEGED_DATA portMUX_TYPE _readerMux = portMUX_INITIALIZER_UNLOCKED;

static const char *TAG = "DHT Sensor";

const SENSOR_INTERFACE_DESCRIPTION * dht_get_inteface()
{
    return &dht_handle_interface_description;
}

int32_t dht_readPulse(uint8_t pin, int32_t usTimeOut, bool state )
{
    int32_t usTime = 0;

    while( gpio_get_level(pin) == state )
    {
        if( usTime++ > usTimeOut )
        {
            return -1;
        }

		ets_delay_us(1);		// uSec delay
	}
	
	return usTime;
}

SENSOR_HANDLE dht_create()
{
    DHT_SENSOR * sensor = malloc(sizeof(DHT_SENSOR));
    sensor->options = malloc(sizeof(DHT_SENSOR_OPTIONS));
    sensor->temperature = 0;
    sensor->humidity = 0;
    sensor->status = DHT_SENSOR_STATUS_CREATED;

    return (SENSOR_HANDLE) sensor;
}

void dht_destroy(SENSOR_HANDLE handle)
{
    DHT_SENSOR * sensor = (DHT_SENSOR *) handle;

    if (sensor != NULL)
    {
        if(sensor->options != NULL)
        {
            free(sensor->options);
        }

        free(sensor);
    }
}

void dht_set_options(SENSOR_HANDLE handle, void * options)
{
    DHT_SENSOR * sensor = (DHT_SENSOR *) handle;
    DHT_SENSOR_OPTIONS * opt = (DHT_SENSOR_OPTIONS *) options; 

    if (sensor != NULL)
    {
        sensor->options->pin = opt->pin;
        sensor->options->type = opt->type;
    }
}

void* dht_get_options(SENSOR_HANDLE handle)
{
    DHT_SENSOR * sensor = (DHT_SENSOR *) handle;
    return (void *) sensor->options;
}

int dht_initialize(SENSOR_HANDLE handle)
{
    vPortCPUInitializeMutex( &_readerMux );
    
    return SENSOR_STATUS_OK;
}

int dht_read(SENSOR_HANDLE handle)
{
    DHT_SENSOR * sensor = (DHT_SENSOR *) handle;
    sensor->status = DHT_SENSOR_STATUS_READY;

    // sensor data is made up of 5 bytes:
    // dht11 has 8 bits for humidity, 8 bits not used, 8 bits for temperature, 8 bits unused and 8-bit crc check
    // the other sensors have 16 bits for humidity, 16 bits for temperature and 8 bit crc check

    uint8_t dht_data[5];       // read 5 bytes (40 bits)
    uint8_t byte_index = 0;    
    uint8_t bit_index = 7;     // start with most significant bit
    uint8_t pin = sensor->options->pin;

    // Clear all bits
    memset(dht_data, 0, sizeof(dht_data));

    taskENTER_CRITICAL(&_readerMux);

    // send start signal
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    
    // Set the line low for 20ms
    gpio_set_level(pin, 0);
    ets_delay_us(20000);

    // Start reading the data.
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    // DHT sensor will keep the line low for ~80us.
    int32_t pulse = dht_readPulse(pin, 85, 0);

    if (pulse <= 0)
    {
        ESP_LOGE(TAG, "Sensor did not respond to start signal: expected low signal for 80us: %d us\n", pulse);
        sensor->status = DHT_SENSOR_STATUS_INVALID_TELEMETRY;
        taskEXIT_CRITICAL(&_readerMux);
        return SENSOR_STATUS_FAILED;
    }

    // Then DHT sensor will keep the line high for ~80us
    pulse = dht_readPulse(pin, 85, 1);

    if (pulse <= 0)
    {
        ESP_LOGE(TAG, "Sensor did not respond to start signal: expected high signal for 80us\n");
        sensor->status = DHT_SENSOR_STATUS_INVALID_TELEMETRY;
        taskEXIT_CRITICAL(&_readerMux);
        return SENSOR_STATUS_FAILED;
    }

    // No errors, read the 40 bits of data
    for (uint8_t bit = 0; bit < 40; ++bit)
    {
        // Each bits are a succession of ~50us low followed by either ~28us high for 0 or ~70us high for 1
        pulse = dht_readPulse(pin, 60, 0);

        if (pulse <= 0)
        {
            ESP_LOGE(TAG, "Error reading data bit: expected low signal for 50us\n");
            sensor->status = DHT_SENSOR_STATUS_INVALID_TELEMETRY;
            taskEXIT_CRITICAL(&_readerMux);
            return ESP_FAIL;
        }

        pulse = dht_readPulse(pin, 75, 1);

        if (pulse <= 0)
        {
            ESP_LOGE(TAG, "Error reading data bit: timed out reading data signal\n");
            sensor->status = DHT_SENSOR_STATUS_INVALID_TELEMETRY;
            taskEXIT_CRITICAL(&_readerMux);
            return SENSOR_STATUS_FAILED;
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

    // all bits have been read. Validate the checksum
    if (((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) & 0xFF) != dht_data[4])
    {
        ESP_LOGE(TAG, "Error reading data bits: checksum did not match data\n");
        sensor->status = DHT_SENSOR_STATUS_INVALID_TELEMETRY;
        return SENSOR_STATUS_FAILED;
    }    
    
    if (sensor->options->type == DHT_11)
    {
        sensor->humidity = dht_data[0];
        sensor->temperature = dht_data[2];
    }
    else
    {
        sensor->humidity = (dht_data[0] * 256 + dht_data[1]) / 10.0;
        sensor->temperature = ((dht_data[2] & 0x7F) * 256 + dht_data[1]) / 10.0;

        if (dht_data[2] &0x80)
        {
            sensor->temperature *= -1;
        }
    }

    return SENSOR_STATUS_OK;
}

int dht_post(SENSOR_HANDLE handle, telemetry_message_handle_t message)
{
    DHT_SENSOR * sensor = (DHT_SENSOR *) handle;
    
    if (sensor->status == DHT_SENSOR_STATUS_READY)
    {
        telemetry_message_add_number(message, "temperature", sensor->temperature);
        telemetry_message_add_number(message, "humidity", sensor->humidity);
        return SENSOR_STATUS_OK;
    }

    return SENSOR_STATUS_FAILED;
}
