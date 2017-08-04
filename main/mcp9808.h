#ifndef __MCP9808_H__
#define __MCP9808_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

#define ESP_ERR_MCP9808_BASE           0x1200
#define ESP_ERR_MCP9808_NOT_RECOGNIZED (ESP_ERR_MCP9808_BASE + 0x01)

/**
 * Opaque pointer type representing mcp9808 device handle
 */
typedef uint32_t mcp9808_handle;

/**
 * @brief Initialize communication with the given mcp9808 device
 * 
 * @param[in]  i2c_address   The i2c address of the MCP9808
 * @param[in]  sda           The gpio pin number used as the i2c's data line.
 *                           This value is only used on the first call. Subsequent calls to init (with different i2c addresses)
 *                           will reuse the same SDA pin value.
 * @param[in]  scl           The gpio pin number used as the i2c's clock line
 *                           This value is only used on the first call. Subsequent calls to init (with different i2c addresses)
 *                           will reuse the same SCL pin value.
 * @param[out] out_handle    The internal device handle returned if return code ESP_OK
 * 
 * @return 
 *          - ESP_OK if the MCP9808 device was successfully initialized
 *          - ESP_ERR_MCP9808_NOT_RECOGNIZED if the MCP9808 was connected but has invalid identification
 *          - other error codes from the underlying i2c driver  
 */
esp_err_t mcp9808_init(uint8_t i2c_address, gpio_num_t sda, gpio_num_t scl, mcp9808_handle * out_handle);

/**
 * @brief Read the temperature from the specified sensor
 * 
 * @param[in]  handle       The internal MCP9808 sensor handle obtained from mcp9808_init
 * @param[out] out_value    The current ambient temperature in celcius if return code is ESP_OK
 *
 * @return
 *           - ESP_OK if the current ambiant temperature was successfully read from the device
 *           - other error codes from the underlying i2c driver
 */
esp_err_t mcp9808_read_temp_c(mcp9808_handle handle, float * out_value);

#ifdef __cplusplus
}
#endif

#endif
