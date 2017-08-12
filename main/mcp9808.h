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
 * @brief Initialize communication with the mcp9808 sensor at specified port/address. A task will be created and
 * sensor telemetry data will be posted on the queue.
 * 
 * @param[in]  i2c_port      The i2c port number
 * @param[in]  i2c_address   The i2c address of the MCP9808
 * @param[in]  telemetry_queue  The queue's handle on which telemetry data will be posted.
 * 
 * @return 
 *          - ESP_OK if the MCP9808 device was successfully initialized
 *          - ESP_ERR_MCP9808_NOT_RECOGNIZED if the MCP9808 was connected but has invalid identification
 *          - other error codes from the underlying i2c driver  
 */
esp_err_t mcp9808_init(i2c_port_t i2c_port, uint8_t i2c_address, QueueHandle_t telemetry_queue);

#ifdef __cplusplus
}
#endif

#endif
