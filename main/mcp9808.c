#include "esp_log.h"

#include "device-config.h"
#include "mcp9808.h"

#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

#define MCP9808_REG_UPPER_TEMP         0x02
#define MCP9808_REG_LOWER_TEMP         0x03
#define MCP9808_REG_CRIT_TEMP          0x04
#define MCP9808_REG_AMBIENT_TEMP       0x05
#define MCP9808_REG_MANUF_ID           0x06
#define MCP9808_REG_DEVICE_ID          0x07

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_address;
    QueueHandle_t telemetry_queue;
} mcp9808_device_t;

static const char *TAG = "mcp9808-hub";

static mcp9808_device_t _config;

// Read 16 bits data from the specified registry
esp_err_t i2c_read_16(uint8_t reg, uint16_t * data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (_config.i2c_address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	
	int status = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (status != ESP_OK) {
        return status;
    }

    vTaskDelay(30 / portTICK_RATE_MS);

    uint8_t data_h, data_l;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_config.i2c_address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);

    i2c_master_read_byte(cmd, &data_h, ACK_VAL);
    i2c_master_read_byte(cmd, &data_l, NACK_VAL);

    i2c_master_stop(cmd);

	status = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    *data = (data_h << 8) + data_l;
    return status;
}


// Loads temperature from mcp9808 sensor into output variable and returns ESP_OK if
// sucessful otherwise return ESP_FAIL
esp_err_t mcp9808_read_temp_c(float * out_value)
{
    uint16_t rawData;
    esp_err_t status = i2c_read_16(MCP9808_REG_AMBIENT_TEMP, &rawData);

    if (status != ESP_OK) {
        return status;
    }

    float temp = rawData & 0x0FFF;
    temp /=  16.0;
    
    if (rawData & 0x1000) {
        temp -= 256;
    }

    *out_value = temp;

    return ESP_OK;
}

void task_poll_sensor_telemetry(void * ptr)
{
    ESP_LOGI(TAG, "Waiting or IoT Hub Connection");

    // Wait until IoT Hub is connected
    xEventGroupWaitBits(_wifi_event_group, IOTHUB_CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Hub Connected. Starting telemetry readings");

    float temperature;

    while(true)
    {
        // Read temperature
        if( mcp9808_read_temp_c(&temperature) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read temperature from sensor\n");
        }

        // Send temperature on telemetry queue
        else if (!xQueueSend(_config.telemetry_queue, &temperature, 500)) {
            ESP_LOGE(TAG, "Failed to send temperature to queue within 500ms\n");
        }

        // Wait for current sampling delay
        vTaskDelay(_device_configuration.sensor_sampling_rate/portTICK_PERIOD_MS);
    }
}

esp_err_t mcp9808_init(i2c_port_t i2c_port, uint8_t i2c_address, QueueHandle_t telemetry_queue)
{
    _config.i2c_port = i2c_port;
    _config.i2c_address = i2c_address;
    _config.telemetry_queue = telemetry_queue;

    uint16_t data = 0;
    esp_err_t status = i2c_read_16(MCP9808_REG_MANUF_ID, &data);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Unable to read manufacturer Id\n");
        return status;
    }

    if (data != 0x0054) {
        ESP_LOGE(TAG, "Invalid manufacturer Id: %x\n", data);
        return ESP_ERR_MCP9808_NOT_RECOGNIZED;
    }

    status = i2c_read_16(MCP9808_REG_DEVICE_ID, &data);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Unable to read device Id\n");
        return status;
    }

    if (data != 0x0400) {
        ESP_LOGE(TAG, "Invalid Device Id\n");
        return ESP_ERR_MCP9808_NOT_RECOGNIZED;
    }

    xTaskCreate(task_poll_sensor_telemetry, "Sensor Polling Thread", 2048, NULL, 5, NULL);

    return ESP_OK;
}
