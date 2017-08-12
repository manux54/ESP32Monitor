#ifndef __IOT_HUB_H__
#define __IOT_HUB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_IOTHUB_BASE           0x1300

/**
 * @brief Initialize iot hub device communication. Start the tasks that read telemetry off the sensors queue
 * and upload data to the Azure's hub 
 * 
 * @param[in]  hostname         The IoT hub's host name
 * @param[in]  device_dd        The IoT hub's device Id.
 * @param[in]  primary_key      The secret device's primary key
 * @param[in]  telemetry_queue  The sensor telemetry messaging queue
 * 
 * @return 
 *          - ESP_OK if the iothub driver has been successfully initialized 
 *          - ESP_FAIL otherwise
 */
esp_err_t iothub_init(const char * hostname, const char * device_id, const char * primary_key, const QueueHandle_t telemetry_queue);

#ifdef __cplusplus
}
#endif

#endif
