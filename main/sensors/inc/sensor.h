#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "telemetry-data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t SENSOR_HANDLE;

#define SENSOR_STATUS_OK           0x0000
#define SENSOR_STATUS_FAILED       0x0001
#define SENSOR_STATUS_TIMEDOUT     0x0002

typedef SENSOR_HANDLE (*SENSOR_CREATE) ();
typedef void (*SENSOR_DESTROY) (SENSOR_HANDLE handle);
typedef void (*SENSOR_SET_OPTIONS) (SENSOR_HANDLE handle, void * options);
typedef void* (*SENSOR_GET_OPTIONS) (SENSOR_HANDLE handle);
typedef int (*SENSOR_INITIALIZE) (SENSOR_HANDLE handle);
typedef int (*SENSOR_READ) (SENSOR_HANDLE handle);
typedef int (*SENSOR_POST_RESULTS) (SENSOR_HANDLE handle, telemetry_message_handle_t message);

typedef struct SENSOR_INTERFACE_DESCRIPTION_TAG
{
    SENSOR_CREATE sensor_create;
    SENSOR_DESTROY sensor_destroy;
    SENSOR_SET_OPTIONS sensor_set_options;
    SENSOR_GET_OPTIONS sensor_get_options;
    SENSOR_INITIALIZE sensor_initialize;
    SENSOR_READ sensor_read;
    SENSOR_POST_RESULTS sensor_post_results;
} SENSOR_INTERFACE_DESCRIPTION;

#ifdef __cplusplus
}
#endif

#endif
