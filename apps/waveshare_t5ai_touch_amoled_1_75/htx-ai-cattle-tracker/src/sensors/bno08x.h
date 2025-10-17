/**
 * @file bno08x.h
 * @brief BNO08x IMU Driver for TuyaOpen SDK
 *
 * Port of SparkFun BNO08x Arduino Library to C for Tuya IoT platform.
 * Provides rotation vector (quaternion) and calculated yaw/pitch/roll.
 *
 * Original Library:
 * https://github.com/sparkfun/SparkFun_BNO08x_Arduino_Library
 *
 * Hardware Connections:
 * - I2C_SCL -> BNO08X SCL
 * - I2C_SDA -> BNO08X SDA
 * - GPIO_INT -> BNO08X INT (optional but recommended)
 * - GPIO_RST -> BNO08X RST (optional but recommended)
 *
 * @copyright Original work Copyright (c) SparkFun Electronics
 * @copyright Modified work Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __BNO08X_H__
#define __BNO08X_H__

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "sh2.h"
#include "sh2_SensorValue.h"
#include "sh2_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

// Default I2C address for BNO08x (0x4B or 0x4A)
#define BNO08X_DEFAULT_ADDRESS 0x4B
// #define BNO08X_ALT_ADDRESS     0x4B

// Sensor report IDs
#define BNO08X_ROTATION_VECTOR                    SH2_ROTATION_VECTOR
#define BNO08X_GAME_ROTATION_VECTOR               SH2_GAME_ROTATION_VECTOR
#define BNO08X_GEOMAGNETIC_ROTATION_VECTOR        SH2_GEOMAGNETIC_ROTATION_VECTOR
#define BNO08X_ACCELEROMETER                      SH2_ACCELEROMETER
#define BNO08X_GYROSCOPE_CALIBRATED               SH2_GYROSCOPE_CALIBRATED
#define BNO08X_MAGNETIC_FIELD_CALIBRATED          SH2_MAGNETIC_FIELD_CALIBRATED
#define BNO08X_LINEAR_ACCELERATION                SH2_LINEAR_ACCELERATION
#define BNO08X_GRAVITY                            SH2_GRAVITY

// Default report intervals (milliseconds)
#define BNO08X_DEFAULT_REPORT_INTERVAL_MS 100  // 10 Hz

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief BNO08x device structure
 */
typedef struct {
    uint8_t i2c_addr;                   // I2C device address
    TUYA_I2C_NUM_E i2c_port;            // I2C port number
    int8_t int_pin;                     // Interrupt pin (-1 if not used)
    int8_t rst_pin;                     // Reset pin (-1 if not used)
    
    sh2_Hal_t hal;                      // SH2 HAL structure
    sh2_ProductIds_t prod_ids;          // Product IDs from sensor
    sh2_SensorValue_t sensor_value;     // Latest sensor value
    
    bool initialized;                   // Initialization complete flag
    bool reset_occurred;                // Reset detection flag
    
    // Q-point values for data conversion
    int16_t rotation_vector_q1;
    int16_t rotation_vector_accuracy_q1;
    int16_t accelerometer_q1;
    int16_t gyro_q1;
    int16_t magnetometer_q1;
} bno08x_dev_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize BNO08x sensor with I2C interface
 * 
 * @param dev Pointer to BNO08x device structure
 * @param i2c_port I2C port number
 * @param i2c_addr I2C device address (0x4A or 0x4B)
 * @param int_pin Interrupt GPIO pin (-1 if not used)
 * @param rst_pin Reset GPIO pin (-1 if not used)
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_init(bno08x_dev_t *dev, TUYA_I2C_NUM_E i2c_port, 
                        uint8_t i2c_addr, int8_t int_pin, int8_t rst_pin);

/**
 * @brief Check if BNO08x is connected and responding
 * 
 * @param dev Pointer to BNO08x device structure
 * @return bool true if connected, false otherwise
 */
bool bno08x_is_connected(bno08x_dev_t *dev);

/**
 * @brief Service the BNO08x sensor (call periodically to read data)
 * 
 * @param dev Pointer to BNO08x device structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_service(bno08x_dev_t *dev);

/**
 * @brief Check if a sensor reset has occurred
 * 
 * @param dev Pointer to BNO08x device structure
 * @return bool true if reset occurred (clears flag)
 */
bool bno08x_was_reset(bno08x_dev_t *dev);

/**
 * @brief Perform hardware reset using RST pin
 * 
 * @param dev Pointer to BNO08x device structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_hardware_reset(bno08x_dev_t *dev);

/**
 * @brief Perform software reset
 * 
 * @param dev Pointer to BNO08x device structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_soft_reset(bno08x_dev_t *dev);

/**
 * @brief Enable rotation vector report
 * 
 * @param dev Pointer to BNO08x device structure
 * @param interval_ms Report interval in milliseconds
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_enable_rotation_vector(bno08x_dev_t *dev, uint16_t interval_ms);

/**
 * @brief Enable game rotation vector report (no magnetometer)
 * 
 * @param dev Pointer to BNO08x device structure
 * @param interval_ms Report interval in milliseconds
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_enable_game_rotation_vector(bno08x_dev_t *dev, uint16_t interval_ms);

/**
 * @brief Enable geomagnetic rotation vector report
 * 
 * @param dev Pointer to BNO08x device structure
 * @param interval_ms Report interval in milliseconds
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_enable_geomagnetic_rotation_vector(bno08x_dev_t *dev, uint16_t interval_ms);

/**
 * @brief Get sensor event (reads latest data)
 * 
 * @param dev Pointer to BNO08x device structure
 * @return bool true if new data available
 */
bool bno08x_get_sensor_event(bno08x_dev_t *dev);

/**
 * @brief Get sensor event ID of last reading
 * 
 * @param dev Pointer to BNO08x device structure
 * @return uint8_t Sensor report ID
 */
uint8_t bno08x_get_sensor_event_id(bno08x_dev_t *dev);

/**
 * @brief Get yaw (heading) in radians from rotation vector
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Yaw angle in radians (-π to +π)
 */
float bno08x_get_yaw(bno08x_dev_t *dev);

/**
 * @brief Get pitch in radians from rotation vector
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Pitch angle in radians
 */
float bno08x_get_pitch(bno08x_dev_t *dev);

/**
 * @brief Get roll in radians from rotation vector
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Roll angle in radians
 */
float bno08x_get_roll(bno08x_dev_t *dev);

/**
 * @brief Get yaw (heading) in degrees from rotation vector
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Yaw angle in degrees (0-360)
 */
float bno08x_get_yaw_degrees(bno08x_dev_t *dev);

/**
 * @brief Get pitch in degrees from rotation vector
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Pitch angle in degrees
 */
float bno08x_get_pitch_degrees(bno08x_dev_t *dev);

/**
 * @brief Get roll in degrees from rotation vector
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Roll angle in degrees
 */
float bno08x_get_roll_degrees(bno08x_dev_t *dev);

/**
 * @brief Get quaternion I component
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Quaternion I value
 */
float bno08x_get_quat_i(bno08x_dev_t *dev);

/**
 * @brief Get quaternion J component
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Quaternion J value
 */
float bno08x_get_quat_j(bno08x_dev_t *dev);

/**
 * @brief Get quaternion K component
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Quaternion K value
 */
float bno08x_get_quat_k(bno08x_dev_t *dev);

/**
 * @brief Get quaternion Real component
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Quaternion Real value
 */
float bno08x_get_quat_real(bno08x_dev_t *dev);

/**
 * @brief Get rotation vector accuracy
 * 
 * @param dev Pointer to BNO08x device structure
 * @return float Accuracy in radians
 */
float bno08x_get_quat_radian_accuracy(bno08x_dev_t *dev);

/**
 * @brief Get rotation vector accuracy status
 * 
 * @param dev Pointer to BNO08x device structure
 * @return uint8_t Accuracy status (0=unreliable, 1=low, 2=medium, 3=high)
 */
uint8_t bno08x_get_quat_accuracy(bno08x_dev_t *dev);

/**
 * @brief Get timestamp of last sensor reading
 * 
 * @param dev Pointer to BNO08x device structure
 * @return uint64_t Timestamp in microseconds
 */
uint64_t bno08x_get_time_stamp(bno08x_dev_t *dev);

/**
 * @brief Enable dynamic calibration for sensors
 * 
 * @param dev Pointer to BNO08x device structure
 * @param sensors Sensor mask (SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG)
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_set_calibration_config(bno08x_dev_t *dev, uint8_t sensors);

/**
 * @brief Save dynamic calibration data to flash
 * 
 * @param dev Pointer to BNO08x device structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_save_calibration(bno08x_dev_t *dev);

/**
 * @brief Perform tare operation (zero heading)
 * 
 * @param dev Pointer to BNO08x device structure
 * @param z_axis_only true to tare only Z axis, false for all axes
 * @param basis Tare basis (rotation vector type)
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_tare_now(bno08x_dev_t *dev, bool z_axis_only, sh2_TareBasis_t basis);

/**
 * @brief Save tare settings to flash
 * 
 * @param dev Pointer to BNO08x device structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_save_tare(bno08x_dev_t *dev);

/**
 * @brief Clear tare settings
 * 
 * @param dev Pointer to BNO08x device structure
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET bno08x_clear_tare(bno08x_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* __BNO08X_H__ */

