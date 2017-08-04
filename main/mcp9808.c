#include "mcp9808.h"

#define I2C_FREQ_HZ    100000     /*!< I2C master clock frequency */

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

typedef struct mcp9808_device
{
    uint8_t i2c_address;
    gpio_num_t sda;
    gpio_num_t scl;
} MCP9808_DEVICE_T;

static MCP9808_DEVICE_T _mcp9808_devices[8] = { 0 };
static uint8_t _next_handle = 0;

// Read 16 byte data from the specified registry
esp_err_t i2c_read_16(uint8_t i2c_address, uint8_t reg, uint16_t * data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (i2c_address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	
	int ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_FAIL)
    {
        puts("Failed to write read request");
        return ret;
    }

    vTaskDelay(30 / portTICK_RATE_MS);

    uint8_t data_h, data_l;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);

    i2c_master_read_byte(cmd, &data_h, ACK_VAL);
    i2c_master_read_byte(cmd, &data_l, NACK_VAL);

    i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(I2C_NUM_1, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    *data = (data_h << 8) + data_l;
    return ret;
}

esp_err_t mcp9808_init(uint8_t i2c_address, gpio_num_t sda, gpio_num_t scl, mcp9808_handle * out_handle)
{
    if (_next_handle > 0)
    {
        // SDA & SCL pin can only be set once. Ignore current values and use previous values
        sda = _mcp9808_devices[1].sda;
        scl = _mcp9808_devices[1].scl;

        // Search if device was already initialized
        for (int i = 1; i <= _next_handle; ++i)
        {
            if (_mcp9808_devices[i].i2c_address == i2c_address)
            {
                *out_handle = i;
                return ESP_OK;
            }
        }
    }

    // Can only initialize 7 different MCP9808 devices
    if (_next_handle == 7)
    {
        return ESP_FAIL;
    }

    int i2c_master_port = I2C_NUM_1;

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;

    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;

    conf.master.clk_speed = I2C_FREQ_HZ;

    i2c_param_config(i2c_master_port, &conf);
    esp_err_t status = i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);

    if (status != ESP_OK) {
        return status; 
    }

    uint16_t data = 0;
    status = i2c_read_16(i2c_address, MCP9808_REG_MANUF_ID, &data);

    if (status != ESP_OK) {
        return status;
    }

    if (data != 0x0054) {
        return ESP_ERR_MCP9808_NOT_RECOGNIZED;
    }

    status = i2c_read_16(i2c_address, MCP9808_REG_DEVICE_ID, &data);

    if (status != ESP_OK) {
        return status;
    }

    if (data != 0x0400) {
        return ESP_ERR_MCP9808_NOT_RECOGNIZED;
    }

    ++_next_handle;
    _mcp9808_devices[_next_handle].sda = sda;
    _mcp9808_devices[_next_handle].scl = scl;
    _mcp9808_devices[_next_handle].i2c_address = i2c_address;

    *out_handle = _next_handle;

    return ESP_OK;
}

esp_err_t mcp9808_read_temp_c(mcp9808_handle handle, float * out_value)
{
    uint16_t rawData;

    if (handle < 1 || handle > _next_handle) {
        return ESP_FAIL;
    }

    esp_err_t status = i2c_read_16(_mcp9808_devices[handle].i2c_address, MCP9808_REG_AMBIENT_TEMP, &rawData);

    if (status == ESP_FAIL) {
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
