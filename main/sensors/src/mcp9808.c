#include "mcp9808.h"

#include "esp_log.h"

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

SENSOR_HANDLE mcp9808_create();
void mcp9808_destroy(SENSOR_HANDLE handle);
void mcp9808_set_options(SENSOR_HANDLE handle, void * options);
void* mcp9808_get_options(SENSOR_HANDLE handle);
int mcp9808_initialize(SENSOR_HANDLE handle);
int mcp9808_read(SENSOR_HANDLE handle);
int mcp9808_post(SENSOR_HANDLE handle, telemetry_message_handle_t message);

static const SENSOR_INTERFACE_DESCRIPTION mcp9808_handle_interface_description =
{
    mcp9808_create,
    mcp9808_destroy,
    mcp9808_set_options,
    mcp9808_get_options,
    mcp9808_initialize,
    mcp9808_read,
    mcp9808_post
};

typedef enum
{
    MCP9808_SENSOR_STATUS_CREATED,
    MCP9808_SENSOR_STATUS_READY,
    MCP9808_SENSOR_STATUS_INVALID_TELEMETRY
} MCP9808_SENSOR_STATUS;

typedef struct MCP9808_SENSOR_TAG
{
    MCP9808_SENSOR_OPTIONS * options;
    float temperature;
    MCP9808_SENSOR_STATUS status;
} MCP9808_SENSOR;

static const char *TAG = "MCP9808 Sensor";

const SENSOR_INTERFACE_DESCRIPTION * mcp9808_get_inteface()
{
    return &mcp9808_handle_interface_description;
}

int i2c_read_16(MCP9808_SENSOR_OPTIONS * options, uint8_t reg, uint16_t * data);

SENSOR_HANDLE mcp9808_create()
{
    MCP9808_SENSOR * sensor = malloc(sizeof(MCP9808_SENSOR));
    sensor->options = malloc(sizeof(MCP9808_SENSOR_OPTIONS));
    sensor->temperature = 0;
    sensor->status = MCP9808_SENSOR_STATUS_CREATED;

    return (SENSOR_HANDLE) sensor;
}

void mcp9808_destroy(SENSOR_HANDLE handle)
{
    MCP9808_SENSOR * sensor = (MCP9808_SENSOR *) handle;

    if (sensor != NULL)
    {
        if(sensor->options != NULL)
        {
            free(sensor->options);
        }

        free(sensor);
    }
}

void mcp9808_set_options(SENSOR_HANDLE handle, void * options)
{
    MCP9808_SENSOR * sensor = (MCP9808_SENSOR *) handle;
    MCP9808_SENSOR_OPTIONS * opt = (MCP9808_SENSOR_OPTIONS *) options; 

    if (sensor != NULL)
    {
        sensor->options->i2c_port = opt->i2c_port;
        sensor->options->i2c_address = opt->i2c_address;
    }
}

void* mcp9808_get_options(SENSOR_HANDLE handle)
{
    MCP9808_SENSOR * sensor = (MCP9808_SENSOR *) handle;
    return (void *) sensor->options;
}

int mcp9808_initialize(SENSOR_HANDLE handle)
{
    MCP9808_SENSOR * sensor = (MCP9808_SENSOR *) handle;

    uint16_t data = 0;
    esp_err_t status = i2c_read_16(sensor->options, MCP9808_REG_MANUF_ID, &data);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Unable to read manufacturer Id\n");
        return SENSOR_STATUS_FAILED;
    }

    if (data != 0x0054) {
        ESP_LOGE(TAG, "Invalid manufacturer Id: %x\n", data);
        return SENSOR_STATUS_FAILED;
    }

    status = i2c_read_16(sensor->options, MCP9808_REG_DEVICE_ID, &data);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Unable to read device Id\n");
        return SENSOR_STATUS_FAILED;
    }

    if (data != 0x0400) {
        ESP_LOGE(TAG, "Invalid Device Id\n");
        return SENSOR_STATUS_FAILED;
    }
    
    return SENSOR_STATUS_OK;
}

int mcp9808_read(SENSOR_HANDLE handle)
{
    uint16_t rawData;
    MCP9808_SENSOR * sensor = (MCP9808_SENSOR *) handle;

    sensor->status = MCP9808_SENSOR_STATUS_READY;
    
    if (i2c_read_16(sensor->options, MCP9808_REG_AMBIENT_TEMP, &rawData) != SENSOR_STATUS_OK) {
        ESP_LOGE(TAG, "Unable to read temperature");
        sensor->status = MCP9808_SENSOR_STATUS_INVALID_TELEMETRY;
        return SENSOR_STATUS_FAILED;
    }

    float temp = rawData & 0x0FFF;
    temp /=  16.0;
    
    if (rawData & 0x1000) {
        temp -= 256;
    }

    sensor->temperature = temp;

    return SENSOR_STATUS_OK;
}

int mcp9808_post(SENSOR_HANDLE handle, telemetry_message_handle_t message)
{
    MCP9808_SENSOR * sensor = (MCP9808_SENSOR *) handle;
    
    if (sensor->status == MCP9808_SENSOR_STATUS_READY)
    {
        telemetry_message_add_number(message, "mcp9808_temperature", sensor->temperature);
        return SENSOR_STATUS_OK;
    }

    return SENSOR_STATUS_FAILED;
}


// Read 16 bits data from the specified registry
int i2c_read_16(MCP9808_SENSOR_OPTIONS * options, uint8_t reg, uint16_t * data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (options->i2c_address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	
	int status = i2c_master_cmd_begin(options->i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (status != ESP_OK) {
        return SENSOR_STATUS_FAILED;
    }

    vTaskDelay(30 / portTICK_RATE_MS);

    uint8_t data_h, data_l;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (options->i2c_address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);

    i2c_master_read_byte(cmd, &data_h, ACK_VAL);
    i2c_master_read_byte(cmd, &data_l, NACK_VAL);

    i2c_master_stop(cmd);

	status = i2c_master_cmd_begin(options->i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    *data = (data_h << 8) + data_l;
    return status == ESP_OK ? SENSOR_STATUS_OK : SENSOR_STATUS_FAILED;
}
