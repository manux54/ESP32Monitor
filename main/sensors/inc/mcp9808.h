#ifndef __MCP9808_H__
#define __MCP9808_H__

#include "sensor.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   The MCP9808 sensor's options. 
 */
typedef struct MCP9808_SENSOR_OPTIONS_TAG
{
    i2c_port_t i2c_port; 
    uint8_t i2c_address;
} MCP9808_SENSOR_OPTIONS;

/**
 * @brief Get the MCP9808 sensor interface
 * 
 * @return 
 *          - The MCP9808 interface including create, options getter and setter, destroy, read and post functions
 */
const SENSOR_INTERFACE_DESCRIPTION * mcp9808_get_inteface();

#ifdef __cplusplus
}
#endif

#endif
