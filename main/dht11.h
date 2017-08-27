#ifndef __DHT11_H__
#define __DHT11_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "esp_err.h"
#include "freertos/queue.h"

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
esp_err_t dht11_init(uint8_t pin, QueueHandle_t telemetry_queue);

#ifdef __cplusplus
}
#endif

#endif
