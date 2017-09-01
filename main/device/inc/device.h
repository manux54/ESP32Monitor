#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "sensor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t DEVICE_HANDLE;

#define DEVICE_STATUS_OK           0x0000
#define DEVICE_STATUS_FAILED       0x0001

/**
 * @brief Create a device. Each device create a thread, read sensors and dispatch sensor telemetry to the iot-hub
 *        through a messaging queue.
 * 
 * @param[in]  deviceId      The device's name
 * @param[in]  telemetry_queue  The queue's handle on which telemetry data will be posted.
 * 
 * @return 
 *          - The device's handle  
 */
DEVICE_HANDLE device_create(const char * deviceId, QueueHandle_t telemetry_queue);

/**
 * @brief Stops the device and dispose of    allocated resources
 *
 * @param[in]  handle        The device for which to dispose the resources
 *
 */
void device_destroy(DEVICE_HANDLE handle);

/**
 * @brief Add a sensor to this device. Each sensor's add its telemetry to the messaging
 *
 * @param[in]  handle            The device's handle from device_create
 * @param[in]  sensor_interface  The sensor's interface
 * @param[in]  sensor_options    The sensor's options to be used in SENSOR_SET_OPTIONS
 *
 * @return
 *          - DEVICE_STATUS_OK if sensor added successfully
 *          - DEVICE_STATUS_FAILED if unable to add sensor
 */
uint32_t device_add_sensor(DEVICE_HANDLE handle, const SENSOR_INTERFACE_DESCRIPTION * sensor_interface, void * sensor_options);
 
/**
 * @brief  Starts reading telemetry from the sensor and send data to the messaging queue.
 *
 * @param[in]  handle          The device's handle from device_create
 * 
 * @return
 *          - DEVICE_STATUS_OK if device started successfully
 *          - DEVICE_STATUS_FAILED if unable to start device
 */
uint32_t device_start(DEVICE_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif
