#include "telemetry-data.h"
#include <stdlib.h>
#include <string.h>

#include "parson.h"
#include "parson.c"

#include "device-config.h"

/**
 * @brief Create a new telemetry message to send to the IoT hub
 * 
 * @return 
 *          - Handle to the telemetry message
 */
telemetry_message_handle_t telemetry_message_create_new()
{
    telemetry_message_handle_t handle = (telemetry_message_handle_t) json_value_init_object();
    return handle;
}

/**
 * @brief Add a boolean result to the telemetry message (e.g. light on or off...)
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 * @param[in]  szKey       The telemetry key 
 * @param[in]  value       The telemetry result's value
 */
 void telemetry_message_add_boolean(telemetry_message_handle_t handle, const char * szKey, bool value)
 {
    JSON_Value * root_value = (JSON_Value *) handle;
    JSON_Object * root_object = json_value_get_object(root_value);
    json_object_set_boolean(root_object, szKey, value);
}

/**
 * @brief Add a number result to the telemetry message (e.g. temperature, pressure, humidity...)
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 * @param[in]  szKey       The telemetry key 
 * @param[in]  value       The telemetry result's value
 */
 void telemetry_message_add_number(telemetry_message_handle_t handle, const char * szKey, double value)
 {
    JSON_Value * root_value = (JSON_Value *) handle;
    JSON_Object * root_object = json_value_get_object(root_value);
    json_object_set_number(root_object, szKey, value);
 }
 
 /**
 * @brief Add a string result to the telemetry message (e.g. value type: celcius, meter... )
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 * @param[in]  szKey       The telemetry key 
 * @param[in]  value       The telemetry result's value
 */
void telemetry_message_add_string(telemetry_message_handle_t handle, const char * szKey, const char * value)
{
    JSON_Value * root_value = (JSON_Value *) handle;
    JSON_Object * root_object = json_value_get_object(root_value);
    json_object_set_string(root_object, szKey, value);
}

/**
 * @brief Convert the telemetry results to Json format
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 *
 * @return
 *            - The serialized json string
 */
 char * telemetry_message_to_json(telemetry_message_handle_t handle)
 {
    JSON_Value * root_value = (JSON_Value *) handle;
    return json_serialize_to_string(root_value);    
 }
 
/**
 * @brief Dispose of the memory allocated for the json output message
 *
 * @param[in]  json      The json output from telemetry_message_to_json
 */
void telemetry_message_dispose_json(char * json)
{
    json_free_serialized_string(json);
}
  
/**
 * @brief Dispose of the memory allocated for the message;
 *
 * @param[in]  handle      The message handle returned from telemetry_message_create_new
 */
void telemetry_message_destroy(telemetry_message_handle_t handle)
{
    JSON_Value * root_value = (JSON_Value *) handle;
    json_value_free(root_value);    
}
