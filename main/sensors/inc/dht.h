#ifndef __DHT_H__
#define __DHT_H__

#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DHT_11,
    DHT_21,
    DHT_22,
    DHT_AM2301
} DHT_TYPE;

/**
 * @brief   The DHT sensor's options. The sensor type (dht11, 21 or 22) and pin number
 */
typedef struct DHT_SENSOR_OPTIONS_TAG
{
    DHT_TYPE type;
    uint8_t pin;
} DHT_SENSOR_OPTIONS;

/**
 * @brief Get the DHT sensor interface
 * 
 * @return 
 *          - The DHT interface including create, options getter and setter, destroy, read and post functions
 */
const SENSOR_INTERFACE_DESCRIPTION * dht_get_inteface();

#ifdef __cplusplus
}
#endif

#endif
