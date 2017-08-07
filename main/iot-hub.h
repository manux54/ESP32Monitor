#ifndef __IOT_HUB_H__
#define __IOT_HUB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_IOTHUB_BASE           0x1300

/**
 * @brief Initialize iot hub device communication
 * 
 * @param[in]  hostname      The IoT hub's host name
 * @param[in]  deviceId      The IoT hub's device Id.
 * @param[in]  primaryKey    The secret device's primary key
 * 
 * @return 
 *          - ESP_OK if the iothub driver has been successfully initialized 
 *          - ESP_FAIL otherwise
 */
esp_err_t iothub_init(const char * hostname, const char * deviceId, const char * primaryKey);

/**
 * @brief Send temperature data to the IoTHub
 *
 * @param[in]  temperature     The temperature recorded by the sensor
 *
 * @return
 *           - ESP_OK if the temperature data was succesfully sent to the azure hub
 *           - ESP_FAIL otherwise
 */
esp_err_t iothub_processCensorData(const float temperature);

/**
 * @brief Send pending message to azure
 *
 * @return
 *         - ESP_OK if pending messages have been sent to the azure hub
 *         - ESP_FAIL otherwise
 */
esp_err_t iothub_processEventQueue();

#ifdef __cplusplus
}
#endif

#endif
