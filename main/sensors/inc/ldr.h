#ifndef __LDR_H__
#define __LDR_H__

#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   The LDR (Light Dependent Resistor) sensor's options. 
 */
typedef struct LDR_SENSOR_OPTIONS_TAG
{
    uint8_t pin;    // GPIO pin 32-39 
} LDR_SENSOR_OPTIONS;

/**
 * @brief   Get the LDR sensor interface
 * 
 * @return 
 *          - The LDR interface including create, options getter and setter, destroy, read and post functions
 */
const SENSOR_INTERFACE_DESCRIPTION * ldr_get_inteface();

#ifdef __cplusplus
}
#endif

#endif
