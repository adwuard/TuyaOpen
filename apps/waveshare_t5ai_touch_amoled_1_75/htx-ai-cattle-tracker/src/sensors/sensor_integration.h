/**
 * @file sensor_integration.h
 * @brief Integration wrapper for BMM150, GPS, and Encoder sensors
 *
 * This header provides unified interfaces for the BMM150 magnetometer, LC76G GPS module,
 * and rotary encoder, making it easy to integrate with the cattle tracker application.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __SENSOR_INTEGRATION_H__
#define __SENSOR_INTEGRATION_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"

// I2C bus coordination mutex for shared I2C Port 0 (GPS + Touch)
extern MUTEX_HANDLE g_i2c_bus_mutex;

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Callback function type for compass accuracy notification (optional)
 * 
 * This callback can be used to monitor BNO08x accuracy status.
 * Note: BNO08x auto-calibrates, no manual calibration needed.
 * 
 * @param accuracy Accuracy level (0=unreliable, 1=low, 2=medium, 3=high)
 */
typedef void (*compass_accuracy_cb_t)(uint8_t accuracy);

typedef struct {
    // BNO08x IMU data (rotation vector with built-in fusion)
    float heading_degrees;     /* Compass heading in degrees (0-360) from rotation vector */
    float pitch_degrees;       /* Pitch angle in degrees */
    float roll_degrees;        /* Roll angle in degrees */
    float quat_i;             /* Quaternion I component */
    float quat_j;             /* Quaternion J component */
    float quat_k;             /* Quaternion K component */
    float quat_real;          /* Quaternion Real component */
    uint8_t quat_accuracy;    /* Quaternion accuracy (0=unreliable, 1=low, 2=medium, 3=high) */
    bool bno08x_ready;        /* BNO08x sensor initialized and ready */
    
    // GPS data
    float latitude_deg;       /* GPS latitude in degrees */
    float longitude_deg;      /* GPS longitude in degrees */
    float altitude_m;         /* GPS altitude in meters */
    float speed_kmh;          /* GPS speed in km/h */
    float course_deg;         /* GPS course in degrees */
    int satellites_in_use;    /* Number of GPS satellites in use */
    int fix_quality;          /* GPS fix quality (0=no fix, 1=GPS fix, 2=DGPS fix) */
    bool gps_ready;           /* GPS sensor initialized and ready */
    
    // Encoder data
    int32_t encoder_angle;    /* Encoder rotation angle (incremental) */
    bool encoder_button;      /* Encoder button state (true=pressed) */
    bool encoder_ready;       /* Encoder initialized and ready */
} sensor_data_t;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialize BNO08x IMU sensor with rotation vector
 *
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET sensor_bno08x_init(void);

/**
 * @brief Initialize LC76G GPS module
 *
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET sensor_gps_init(void);

/**
 * @brief Initialize rotary encoder input
 *
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET sensor_encoder_init(void);

/**
 * @brief Start sensor reading tasks
 *
 * This starts background tasks for both BNO08x and GPS reading.
 *
 * @return OPERATE_RET Operation result, OPRT_OK indicates success
 */
OPERATE_RET sensor_tasks_start(void);

/**
 * @brief Get current sensor data
 *
 * @param data Pointer to sensor_data_t structure to fill with current readings
 * @return OPERATE_RET Operation result, OPRT_OK indicates success
 */
OPERATE_RET sensor_get_data(sensor_data_t *data);

/**
 * @brief Print sensor readings to console (for debugging)
 *
 * This prints BNO08x and GPS readings to the console in a formatted way.
 */
void sensor_print_readings(void);

/**
 * @brief Register a callback for compass accuracy monitoring (optional)
 *
 * Note: BNO08x auto-calibrates. This callback is optional for monitoring only.
 *
 * @param callback Callback function pointer (NULL to unregister)
 * @return OPERATE_RET Operation result, OPRT_OK indicates success
 */
OPERATE_RET sensor_bno08x_register_accuracy_cb(compass_accuracy_cb_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_INTEGRATION_H__ */

