#ifndef __TELEMETRY_DATA_H__
#define __TELEMETRY_DATA_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t telemetry_message_handle_t;

/**
 * @brief Create a new telemetry message to send to the IoT hub
 * 
 * @return 
 *          - Handle to the telemetry message
 */
telemetry_message_handle_t telemetry_message_create_new();

/**
 * @brief Add a boolean result to the telemetry message (e.g. light on or off...)
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 * @param[in]  szKey       The telemetry key 
 * @param[in]  value       The telemetry result's value
 */
void telemetry_message_add_boolean(telemetry_message_handle_t handle, const char * szKey, bool value);
 
/**
 * @brief Add a number result to the telemetry message (e.g. temperature, pressure, humidity...)
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 * @param[in]  szKey       The telemetry key 
 * @param[in]  value       The telemetry result's value
 */
void telemetry_message_add_number(telemetry_message_handle_t handle, const char * szKey, double value);
 
 /**
 * @brief Add a string result to the telemetry message (e.g. value type: celcius, meter... )
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 * @param[in]  szKey       The telemetry key 
 * @param[in]  value       The telemetry result's value
 */
void telemetry_message_add_string(telemetry_message_handle_t handle, const char * szKey, const char * value);

/**
 * @brief Convert the telemetry results to Json format
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 *
 * @return
 *            - The serialized json string
 */
char * telemetry_message_to_json(telemetry_message_handle_t handle);

 /**
  * @brief Dispose of the memory allocated for the json output message
  *
  * @param[in]  json      The json output from telemetry_message_to_json
  */
void telemetry_message_dispose_json(char * json);

 /**
  * @brief Dispose of the memory allocated for the message;
  *
  * @param[in]  handle      The message handle returned from telemetry_message_create_new
  */
void telemetry_message_destroy(telemetry_message_handle_t handle);
 
#ifdef __cplusplus
}
#endif

#endif
