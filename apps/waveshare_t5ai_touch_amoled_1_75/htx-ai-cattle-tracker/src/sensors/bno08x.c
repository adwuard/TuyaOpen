/**
 * @file bno08x.c
 * @brief BNO08x IMU Driver Implementation for TuyaOpen SDK
 *
 * Port of SparkFun BNO08x Arduino Library to C for Tuya IoT platform.
 *
 * This implementation follows the Arduino library patterns exactly to ensure
 * proper communication with the BNO08x sensor via SHTP over I2C protocol.
 *
 * Key Features:
 * - GPIO initialization for INT and RST pins (matches Arduino library)
 * - Hardware reset sequence (HIGH->LOW->HIGH timing)
 * - INT pin monitoring with 500ms timeout and recovery
 * - SHTP protocol implementation with proper packet validation
 * - Comprehensive debug logging for troubleshooting
 * - Initialization sequence matches proven Arduino library
 *
 * Critical Implementation Details:
 * 1. INT pin: Configured as INPUT with pull-up, waits for LOW (sensor ready)
 * 2. RST pin: Configured as OUTPUT, performs reset sequence
 * 3. I2C: Uses SHTP protocol (not register-based)
 * 4. Timing: Follows datasheet specs (300ms reset, 500ms INT timeout)
 *
 * @copyright Original work Copyright (c) SparkFun Electronics
 * @copyright Modified work Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#include "bno08x.h"
#include "dev_config.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tkl_i2c.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"
#include <math.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define I2C_BUFFER_MAX_SIZE 128  // Maximum I2C buffer size for Tuya platform

// Conversion factors
#define RADIANS_TO_DEGREES (180.0f / 3.14159265358979323846f)
#define DEGREES_TO_RADIANS (3.14159265358979323846f / 180.0f)

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

// Static module variables for HAL callbacks
static bno08x_dev_t *g_active_device = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

// Forward declarations for HAL functions
static int i2c_hal_open(sh2_Hal_t *self);
static void i2c_hal_close(sh2_Hal_t *self);
static int i2c_hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
static int i2c_hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
static uint32_t hal_get_time_us(sh2_Hal_t *self);
static void hal_callback(void *cookie, sh2_AsyncEvent_t *pEvent);
static void sensor_handler(void *cookie, sh2_SensorEvent_t *pEvent);

// I2C helper functions
static bool i2c_write_bytes(bno08x_dev_t *dev, const uint8_t *buffer, size_t len);
static bool i2c_read_bytes(bno08x_dev_t *dev, uint8_t *buffer, size_t len);
static bool wait_for_int(bno08x_dev_t *dev);
static void hardware_reset_impl(bno08x_dev_t *dev);

// Note: Q-point conversion not needed - sh2_SensorValue provides float values directly

/**
 * @brief Initialize BNO08x sensor
 */
OPERATE_RET bno08x_init(bno08x_dev_t *dev, TUYA_I2C_NUM_E i2c_port, 
                        uint8_t i2c_addr, int8_t int_pin, int8_t rst_pin)
{
    if (!dev) {
        PR_ERR("BNO08x: Invalid device pointer");
        return OPRT_INVALID_PARM;
    }

    memset(dev, 0, sizeof(bno08x_dev_t));
    
    dev->i2c_port = i2c_port;
    dev->i2c_addr = i2c_addr;
    dev->int_pin = int_pin;  // Use parameter (pass -1 to disable)
    dev->rst_pin = rst_pin;  // Use parameter (pass -1 to disable)
    dev->initialized = false;
    dev->reset_occurred = false;
    
    // Initialize Q-point values (from BNO08x datasheet)
    dev->rotation_vector_q1 = 14;
    dev->rotation_vector_accuracy_q1 = 12;
    dev->accelerometer_q1 = 8;
    dev->gyro_q1 = 9;
    dev->magnetometer_q1 = 4;

    // Set the active device for HAL callbacks
    g_active_device = dev;

    // Configure I2C pinmux for GPIO 14/15 on I2C Port 2
    PR_INFO("BNO08x: Configuring I2C pinmux for GPIO 14 (SCL) and GPIO 15 (SDA)");
    tkl_io_pinmux_config(BNO08X_I2C_SCL_PIN_NUM, TUYA_IIC2_SCL);
    tkl_io_pinmux_config(BNO08X_I2C_SDA_PIN_NUM, TUYA_IIC2_SDA);

    // Initialize I2C port with proper configuration
    // Try to deinit first in case it was already initialized
    tkl_i2c_deinit(dev->i2c_port);
    tal_system_sleep(10);  // Small delay after deinit
    
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
        .speed = TUYA_IIC_BUS_SPEED_100K  // BNO08x supports up to 400kHz
    };
    
    OPERATE_RET ret = tkl_i2c_init(dev->i2c_port, &i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("BNO08x: I2C init failed on port %d (error: %d)", dev->i2c_port, ret);
        PR_ERR("BNO08x: This may indicate the I2C port is in use or invalid");
        return ret;
    }
    
    // Give I2C bus time to stabilize
    tal_system_sleep(50);
    
    // Perform a dummy read to clear any bus noise/garbage
    // This helps ensure clean communication with the sensor
    // Use xfer_pending=FALSE to send STOP condition
    uint8_t dummy_buf[4];
    tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, dummy_buf, 4, FALSE);
    tal_system_sleep(10);
    
    PR_INFO("BNO08x: I2C initialized on Port %d (GPIO 14/15) at 100kHz", dev->i2c_port);

    // Log pin configuration
    if (dev->int_pin >= 0) {
        PR_INFO("BNO08x: Using INT pin GPIO%d for interrupt detection", dev->int_pin);
    } else {
        PR_INFO("BNO08x: INT pin disabled - polling mode only");
    }
    
    if (dev->rst_pin >= 0) {
        PR_INFO("BNO08x: Using RST pin GPIO%d for hardware reset", dev->rst_pin);
    } else {
        PR_INFO("BNO08x: RST pin disabled - software reset only");
    }

    // Configure GPIO pins if provided
    if (dev->int_pin >= 0) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .mode = TUYA_GPIO_INPUT,
            .direct = TUYA_GPIO_INPUT,
            .level = TUYA_GPIO_LEVEL_HIGH  // Pull-up
        };
        OPERATE_RET ret = tkl_gpio_init(dev->int_pin, &gpio_cfg);
        if (ret != OPRT_OK) {
            PR_ERR("BNO08x: Failed to initialize INT pin GPIO%d (error: %d)", dev->int_pin, ret);
            return ret;
        }
        PR_INFO("BNO08x: INT pin GPIO%d configured as INPUT with pull-up", dev->int_pin);
    }

    if (dev->rst_pin >= 0) {
        // Initialize as OUTPUT with HIGH level (idle state)
        TUYA_GPIO_BASE_CFG_T gpio_cfg = {
            .mode = TUYA_GPIO_OUTPUT,
            .direct = TUYA_GPIO_OUTPUT,
            .level = TUYA_GPIO_LEVEL_HIGH
        };
        OPERATE_RET ret = tkl_gpio_init(dev->rst_pin, &gpio_cfg);
        if (ret != OPRT_OK) {
            PR_ERR("BNO08x: Failed to initialize RST pin GPIO%d (error: %d)", dev->rst_pin, ret);
            return ret;
        }
        PR_INFO("BNO08x: RST pin GPIO%d configured as OUTPUT", dev->rst_pin);
    }

    // Small delay after GPIO initialization
    tal_system_sleep(10);

    // Wait for INT pin if configured (like Arduino library does in begin())
    if (dev->int_pin >= 0) {
        PR_INFO("BNO08x: Waiting for INT pin to be ready...");
        if (!wait_for_int(dev)) {
            PR_WARN("BNO08x: INT pin timeout during init, continuing anyway");
        }
    }

    // Check if device is connected
    if (!bno08x_is_connected(dev)) {
        PR_ERR("BNO08x: Device not responding at address 0x%02X", dev->i2c_addr);
        return OPRT_COM_ERROR;
    }

    PR_INFO("BNO08x: I2C device found at address 0x%02X", dev->i2c_addr);

    // Setup HAL interface
    dev->hal.open = i2c_hal_open;
    dev->hal.close = i2c_hal_close;
    dev->hal.read = i2c_hal_read;
    dev->hal.write = i2c_hal_write;
    dev->hal.getTimeUs = hal_get_time_us;

    // Perform hardware reset (like Arduino library)
    if (dev->rst_pin >= 0) {
        PR_INFO("BNO08x: Performing hardware reset");
        hardware_reset_impl(dev);
        tal_system_sleep(100);  // Wait for reset to complete
    } else {
        // No RST pin: wait for sensor power-on boot sequence
        // BNO08X boot time from power-on is 104-678ms (datasheet)
        // Wait conservatively for max boot time + margin
        PR_INFO("BNO08x: No RST pin - waiting for sensor boot sequence (800ms)");
        tal_system_sleep(800);
    }
    
    // Additional stabilization time for sensor to be fully ready for I2C
    tal_system_sleep(100);

    // Open SH2 interface
    int status = sh2_open(&dev->hal, hal_callback, NULL);
    if (status != SH2_OK) {
        PR_ERR("BNO08x: Failed to open SH2 interface (error: %d)", status);
        return OPRT_COM_ERROR;
    }
    
    PR_INFO("BNO08x: SH2 interface opened successfully");

    // Get product IDs (optional - like Arduino library)
    memset(&dev->prod_ids, 0, sizeof(dev->prod_ids));
    status = sh2_getProdIds(&dev->prod_ids);
    if (status != SH2_OK) {
        PR_WARN("BNO08x: Could not get product IDs (error: %d) - continuing anyway", status);
    } else {
        PR_INFO("BNO08x: Product ID retrieved successfully");
    }

    // Register sensor callback
    sh2_setSensorCallback(sensor_handler, NULL);

    dev->initialized = true;
    PR_INFO("BNO08x: Initialization complete - ready for sensor reports");
    
    return OPRT_OK;
}

/**
 * @brief Check if BNO08x is connected
 */
bool bno08x_is_connected(bno08x_dev_t *dev)
{
    if (!dev) return false;
    
    // Try to address the device to check if it responds with ACK
    // Send a dummy byte with STOP condition (xfer_pending=FALSE)
    uint8_t dummy = 0;
    OPERATE_RET ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &dummy, 1, FALSE);
    
    PR_DEBUG("BNO08x isConnected check: ret=%d (addr=0x%02X)", ret, dev->i2c_addr);
    return (ret == OPRT_OK);
}

/**
 * @brief Service the BNO08x sensor
 */
OPERATE_RET bno08x_service(bno08x_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    sh2_service();
    return OPRT_OK;
}

/**
 * @brief Check if reset occurred
 */
bool bno08x_was_reset(bno08x_dev_t *dev)
{
    if (!dev) return false;
    
    bool reset = dev->reset_occurred;
    dev->reset_occurred = false;
    return reset;
}

/**
 * @brief Perform hardware reset
 */
OPERATE_RET bno08x_hardware_reset(bno08x_dev_t *dev)
{
    if (!dev) return OPRT_INVALID_PARM;
    
    hardware_reset_impl(dev);
    return OPRT_OK;
}

/**
 * @brief Perform software reset
 */
OPERATE_RET bno08x_soft_reset(bno08x_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    int status = sh2_devReset();
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Enable rotation vector report
 */
OPERATE_RET bno08x_enable_rotation_vector(bno08x_dev_t *dev, uint16_t interval_ms)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    // Use static config like Arduino library (important for proper initialization)
    static sh2_SensorConfig_t config;
    memset(&config, 0, sizeof(config));
    
    // These sensor options are disabled or not used in most cases
    config.changeSensitivityEnabled = false;
    config.wakeupEnabled = false;
    config.changeSensitivityRelative = false;
    config.alwaysOnEnabled = false;
    config.changeSensitivity = 0;
    config.batchInterval_us = 0;
    config.sensorSpecific = 0;
    
    // Convert ms to microseconds
    config.reportInterval_us = (uint32_t)interval_ms * 1000;
    
    // Wait for INT pin if configured (like Arduino library)
    if (dev->int_pin >= 0) {
        if (!wait_for_int(dev)) {
            PR_WARN("BNO08x: Timeout waiting for INT, continuing anyway");
        }
    }
    
    PR_INFO("BNO08x: Configuring rotation vector with %d ms (%u us) interval", 
            interval_ms, config.reportInterval_us);
    
    int status = sh2_setSensorConfig(SH2_ROTATION_VECTOR, &config);
    if (status != SH2_OK) {
        PR_ERR("BNO08x: Failed to enable rotation vector (sh2 error: %d)", status);
        return OPRT_COM_ERROR;
    }
    
    PR_INFO("BNO08x: Rotation vector enabled successfully");
    return OPRT_OK;
}

/**
 * @brief Enable game rotation vector report
 */
OPERATE_RET bno08x_enable_game_rotation_vector(bno08x_dev_t *dev, uint16_t interval_ms)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    sh2_SensorConfig_t config;
    memset(&config, 0, sizeof(config));
    
    config.reportInterval_us = (uint32_t)interval_ms * 1000;
    
    int status = sh2_setSensorConfig(SH2_GAME_ROTATION_VECTOR, &config);
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Enable geomagnetic rotation vector report
 */
OPERATE_RET bno08x_enable_geomagnetic_rotation_vector(bno08x_dev_t *dev, uint16_t interval_ms)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    sh2_SensorConfig_t config;
    memset(&config, 0, sizeof(config));
    
    config.reportInterval_us = (uint32_t)interval_ms * 1000;
    
    int status = sh2_setSensorConfig(SH2_GEOMAGNETIC_ROTATION_VECTOR, &config);
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Get sensor event
 */
bool bno08x_get_sensor_event(bno08x_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return false;
    }
    
    g_active_device = dev;
    dev->sensor_value.timestamp = 0;
    
    sh2_service();
    
    if (dev->sensor_value.timestamp == 0 && 
        dev->sensor_value.sensorId != SH2_GYRO_INTEGRATED_RV) {
        return false;  // No new events
    }
    
    return true;
}

/**
 * @brief Get sensor event ID
 */
uint8_t bno08x_get_sensor_event_id(bno08x_dev_t *dev)
{
    if (!dev) return 0;
    return dev->sensor_value.sensorId;
}

/**
 * @brief Get yaw in radians
 */
float bno08x_get_yaw(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    
    float dqw = dev->sensor_value.un.rotationVector.real;
    float dqx = dev->sensor_value.un.rotationVector.i;
    float dqy = dev->sensor_value.un.rotationVector.j;
    float dqz = dev->sensor_value.un.rotationVector.k;
    
    // Normalize quaternion
    float norm = sqrtf(dqw*dqw + dqx*dqx + dqy*dqy + dqz*dqz);
    if (norm == 0.0f) return 0.0f;
    
    dqw /= norm;
    dqx /= norm;
    dqy /= norm;
    dqz /= norm;
    
    float ysqr = dqy * dqy;
    
    // Yaw (z-axis rotation)
    float t3 = 2.0f * (dqw * dqz + dqx * dqy);
    float t4 = 1.0f - 2.0f * (ysqr + dqz * dqz);
    float yaw = atan2f(t3, t4);
    
    return yaw;
}

/**
 * @brief Get pitch in radians
 */
float bno08x_get_pitch(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    
    float dqw = dev->sensor_value.un.rotationVector.real;
    float dqx = dev->sensor_value.un.rotationVector.i;
    float dqy = dev->sensor_value.un.rotationVector.j;
    float dqz = dev->sensor_value.un.rotationVector.k;
    
    // Normalize quaternion
    float norm = sqrtf(dqw*dqw + dqx*dqx + dqy*dqy + dqz*dqz);
    if (norm == 0.0f) return 0.0f;
    
    dqw /= norm;
    dqx /= norm;
    dqy /= norm;
    dqz /= norm;
    
    // Pitch (y-axis rotation)
    float t2 = 2.0f * (dqw * dqy - dqz * dqx);
    t2 = (t2 > 1.0f) ? 1.0f : t2;
    t2 = (t2 < -1.0f) ? -1.0f : t2;
    float pitch = asinf(t2);
    
    return pitch;
}

/**
 * @brief Get roll in radians
 */
float bno08x_get_roll(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    
    float dqw = dev->sensor_value.un.rotationVector.real;
    float dqx = dev->sensor_value.un.rotationVector.i;
    float dqy = dev->sensor_value.un.rotationVector.j;
    float dqz = dev->sensor_value.un.rotationVector.k;
    
    // Normalize quaternion
    float norm = sqrtf(dqw*dqw + dqx*dqx + dqy*dqy + dqz*dqz);
    if (norm == 0.0f) return 0.0f;
    
    dqw /= norm;
    dqx /= norm;
    dqy /= norm;
    dqz /= norm;
    
    float ysqr = dqy * dqy;
    
    // Roll (x-axis rotation)
    float t0 = 2.0f * (dqw * dqx + dqy * dqz);
    float t1 = 1.0f - 2.0f * (dqx * dqx + ysqr);
    float roll = atan2f(t0, t1);
    
    return roll;
}

/**
 * @brief Get yaw in degrees (0-360)
 */
float bno08x_get_yaw_degrees(bno08x_dev_t *dev)
{
    float yaw_rad = bno08x_get_yaw(dev);
    float yaw_deg = yaw_rad * RADIANS_TO_DEGREES;
    
    // Convert from -180..+180 to 0..360
    if (yaw_deg < 0.0f) {
        yaw_deg += 360.0f;
    }
    
    return yaw_deg;
}

/**
 * @brief Get pitch in degrees
 */
float bno08x_get_pitch_degrees(bno08x_dev_t *dev)
{
    return bno08x_get_pitch(dev) * RADIANS_TO_DEGREES;
}

/**
 * @brief Get roll in degrees
 */
float bno08x_get_roll_degrees(bno08x_dev_t *dev)
{
    return bno08x_get_roll(dev) * RADIANS_TO_DEGREES;
}

/**
 * @brief Get quaternion I
 */
float bno08x_get_quat_i(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    return dev->sensor_value.un.rotationVector.i;
}

/**
 * @brief Get quaternion J
 */
float bno08x_get_quat_j(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    return dev->sensor_value.un.rotationVector.j;
}

/**
 * @brief Get quaternion K
 */
float bno08x_get_quat_k(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    return dev->sensor_value.un.rotationVector.k;
}

/**
 * @brief Get quaternion Real
 */
float bno08x_get_quat_real(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    return dev->sensor_value.un.rotationVector.real;
}

/**
 * @brief Get radian accuracy
 */
float bno08x_get_quat_radian_accuracy(bno08x_dev_t *dev)
{
    if (!dev) return 0.0f;
    return dev->sensor_value.un.rotationVector.accuracy;
}

/**
 * @brief Get accuracy status
 */
uint8_t bno08x_get_quat_accuracy(bno08x_dev_t *dev)
{
    if (!dev) return 0;
    return dev->sensor_value.status;
}

/**
 * @brief Get timestamp
 */
uint64_t bno08x_get_time_stamp(bno08x_dev_t *dev)
{
    if (!dev) return 0;
    return dev->sensor_value.timestamp;
}

/**
 * @brief Set calibration config
 */
OPERATE_RET bno08x_set_calibration_config(bno08x_dev_t *dev, uint8_t sensors)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    int status = sh2_setCalConfig(sensors);
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Save calibration
 */
OPERATE_RET bno08x_save_calibration(bno08x_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    int status = sh2_saveDcdNow();
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Tare now
 */
OPERATE_RET bno08x_tare_now(bno08x_dev_t *dev, bool z_axis_only, sh2_TareBasis_t basis)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    uint8_t axes = z_axis_only ? SH2_TARE_Z : (SH2_TARE_X | SH2_TARE_Y | SH2_TARE_Z);
    int status = sh2_setTareNow(axes, basis);
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Save tare
 */
OPERATE_RET bno08x_save_tare(bno08x_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    int status = sh2_persistTare();
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Clear tare
 */
OPERATE_RET bno08x_clear_tare(bno08x_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    int status = sh2_clearTare();
    if (status != SH2_OK) {
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/***********************************************************
********************* HAL Implementation ********************
*
* SHTP (Sensor Hub Transport Protocol) over I2C:
* ------------------------------------------------
* The BNO08X uses SHTP over I2C, which works differently than
* standard register-based I2C communication:
* 
* WRITE: 
*   - Send packet data directly (no register address)
*   - Format: [len_low, len_high, channel, seq, ...data]
* 
* READ:
*   1. Read 4-byte header to get packet size
*   2. Read packet_size bytes (includes the header again)
*   3. For multi-chunk reads, each subsequent chunk has a header
* 
* This implementation follows the SparkFun Arduino library
* pattern exactly to ensure compatibility.
*
***********************************************************/

/**
 * @brief I2C HAL open - follows Arduino library pattern exactly
 * Sends a soft reset command to the sensor during initialization
 */
static int i2c_hal_open(sh2_Hal_t *self)
{
    bno08x_dev_t *dev = g_active_device;
    if (!dev) {
        PR_ERR("BNO08x: HAL open - no active device");
        return -1;
    }
    
    PR_INFO("BNO08x: HAL open - initializing SHTP interface");
    
    // Wait for INT if configured
    if (dev->int_pin >= 0) {
        PR_DEBUG("BNO08x: HAL open - waiting for INT pin");
        if (!wait_for_int(dev)) {
            PR_WARN("BNO08x: HAL open - INT timeout, continuing anyway");
        }
    }
    
    // Send soft reset packet: {length_low, length_high, channel, seq, reset_cmd}
    // This is the SHTP soft reset command packet format
    uint8_t softreset_pkt[] = {5, 0, 1, 0, 1};
    bool success = false;
    
    PR_INFO("BNO08x: HAL open - sending soft reset command");
    
    // Try up to 5 times to send the reset command (like Arduino library)
    for (uint8_t attempts = 0; attempts < 5; attempts++) {
        if (attempts > 0) {
            PR_DEBUG("BNO08x: HAL open - retry %u/5", attempts + 1);
        }
        
        if (i2c_write_bytes(dev, softreset_pkt, 5)) {
            success = true;
            PR_INFO("BNO08x: HAL open - soft reset command sent successfully");
            break;
        }
        tal_system_sleep(30);
    }
    
    if (!success) {
        PR_ERR("BNO08x: HAL open - failed to send soft reset after 5 attempts");
        return -1;
    }
    
    // Wait for device to complete reset (like Arduino library)
    PR_DEBUG("BNO08x: HAL open - waiting 300ms for reset to complete");
    tal_system_sleep(300);
    
    PR_INFO("BNO08x: HAL open - SHTP interface initialized successfully");
    return 0;
}

/**
 * @brief I2C HAL close
 */
static void i2c_hal_close(sh2_Hal_t *self)
{
    // Nothing to do for Tuya platform
}

/**
 * @brief I2C HAL read - follows Arduino library pattern exactly
 * SHTP over I2C protocol: 
 * 1. Read 4-byte header to get packet size
 * 2. Read packet_size bytes (includes header again)
 */
static int i2c_hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
{
    bno08x_dev_t *dev = g_active_device;
    if (!dev || !pBuffer) {
        return 0;
    }
    
    // Wait for INT if configured
    if (dev->int_pin >= 0) {
        if (!wait_for_int(dev)) {
            return 0;
        }
    }
    
    // Step 1: Read 4-byte header to determine packet size
    uint8_t header[4];
    if (!i2c_read_bytes(dev, header, 4)) {
        PR_DEBUG("BNO08x: HAL read - failed to read header");
        return 0;  // No data available or read failed
    }
    
    // Extract packet size from header (little-endian)
    uint16_t packet_size = (uint16_t)header[0] | ((uint16_t)header[1] << 8);
    bool continue_bit = (packet_size & 0x8000) != 0;
    // Clear the "continue" bit
    packet_size &= ~0x8000;
    
    PR_DEBUG("BNO08x: HAL read - header: [0x%02X 0x%02X 0x%02X 0x%02X] size=%u continue=%d", 
             header[0], header[1], header[2], header[3], packet_size, continue_bit);
    
    // Validate packet size - be strict to avoid processing garbage
    // SHTP packets must be at least 4 bytes (header) and reasonable in size
    if (packet_size == 0 || packet_size < 4) {
        PR_DEBUG("BNO08x: HAL read - invalid packet size %u (too small)", packet_size);
        return 0;  // Invalid or no data (sensor not ready)
    }
    
    if (packet_size > len) {
        // Packet too large for buffer - likely garbage data
        PR_WARN("BNO08x: HAL read - packet size %u exceeds buffer length %u", packet_size, len);
        return 0;
    }
    
    // Strict sanity check: SHTP packets are typically < 256 bytes
    // Anything larger is likely garbage from bus noise or sensor not ready
    if (packet_size > 384) {  // Our buffer max size
        PR_WARN("BNO08x: HAL read - packet size %u exceeds max (384), likely garbage", packet_size);
        return 0;
    }
    
    // Step 2: Read the full packet in chunks
    // cargo_remaining is the number of bytes left to read from the FULL packet
    uint16_t cargo_remaining = packet_size;
    uint8_t i2c_buffer[I2C_BUFFER_MAX_SIZE];
    bool first_read = true;
    
    while (cargo_remaining > 0) {
        uint16_t read_size;
        uint16_t cargo_read_amount;
        
        // Calculate how much to read this iteration
        if (first_read) {
            // First read: read up to I2C_BUFFER_MAX_SIZE of the packet
            read_size = (cargo_remaining < I2C_BUFFER_MAX_SIZE) ? cargo_remaining : I2C_BUFFER_MAX_SIZE;
        } else {
            // Subsequent reads: each chunk has a 4-byte header + cargo data
            // So we need to read (remaining_cargo + 4) bytes, up to buffer max
            uint16_t chunk_size = cargo_remaining + 4;
            read_size = (chunk_size < I2C_BUFFER_MAX_SIZE) ? chunk_size : I2C_BUFFER_MAX_SIZE;
        }
        
        // Wait for INT if configured
        if (dev->int_pin >= 0) {
            if (!wait_for_int(dev)) {
                return 0;
            }
        }
        
        // Read the chunk
        if (!i2c_read_bytes(dev, i2c_buffer, read_size)) {
            return 0;
        }
        
        // Copy data to output buffer
        if (first_read) {
            // First read: copy all bytes (this is the packet data including header)
            cargo_read_amount = read_size;
            memcpy(pBuffer, i2c_buffer, cargo_read_amount);
            first_read = false;
        } else {
            // Subsequent reads: skip the 4-byte header, copy only cargo
            cargo_read_amount = read_size - 4;
            memcpy(pBuffer, i2c_buffer + 4, cargo_read_amount);
        }
        
        // Advance buffer pointer and decrease remaining count
        pBuffer += cargo_read_amount;
        cargo_remaining -= cargo_read_amount;
    }
    
    PR_DEBUG("BNO08x: HAL read - successfully read %u bytes", packet_size);
    return packet_size;
}

/**
 * @brief I2C HAL write - follows Arduino library pattern exactly
 */
static int i2c_hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
{
    bno08x_dev_t *dev = g_active_device;
    if (!dev || !pBuffer) {
        PR_DEBUG("BNO08x: HAL write - invalid parameters");
        return 0;
    }
    
    // Limit write size to I2C buffer max (like Arduino library)
    uint16_t write_size = (len < I2C_BUFFER_MAX_SIZE) ? len : I2C_BUFFER_MAX_SIZE;
    
    PR_DEBUG("BNO08x: HAL write - attempting to write %u bytes (requested %u)", write_size, len);
    
    // Wait for INT if configured
    if (dev->int_pin >= 0) {
        if (!wait_for_int(dev)) {
            PR_DEBUG("BNO08x: HAL write - INT timeout");
            return 0;
        }
    }
    
    // Write data
    if (!i2c_write_bytes(dev, pBuffer, write_size)) {
        PR_DEBUG("BNO08x: HAL write - I2C write failed");
        return 0;
    }
    
    PR_DEBUG("BNO08x: HAL write - successfully wrote %u bytes", write_size);
    return write_size;
}

/**
 * @brief Get time in microseconds
 */
static uint32_t hal_get_time_us(sh2_Hal_t *self)
{
    return tal_system_get_millisecond() * 1000;
}

/**
 * @brief HAL callback for async events
 */
static void hal_callback(void *cookie, sh2_AsyncEvent_t *pEvent)
{
    bno08x_dev_t *dev = g_active_device;
    if (!dev) return;
    
    if (pEvent->eventId == SH2_RESET) {
        dev->reset_occurred = true;
        PR_INFO("BNO08x: Reset detected");
    }
}

/**
 * @brief Sensor event handler
 */
static void sensor_handler(void *cookie, sh2_SensorEvent_t *pEvent)
{
    bno08x_dev_t *dev = g_active_device;
    if (!dev) return;
    
    int rc = sh2_decodeSensorEvent(&dev->sensor_value, pEvent);
    if (rc != SH2_OK) {
        dev->sensor_value.timestamp = 0;
        return;
    }
}

/**
 * @brief Write bytes via I2C - follows Arduino Wire.write() pattern
 * Arduino: beginTransmission() + write() + endTransmission(stop=true)
 * Tuya: tkl_i2c_master_send(..., xfer_pending=FALSE) sends STOP
 */
static bool i2c_write_bytes(bno08x_dev_t *dev, const uint8_t *buffer, size_t len)
{
    if (!dev || !buffer || len == 0) {
        PR_DEBUG("BNO08x: i2c_write_bytes - invalid parameters");
        return false;
    }
    
    // SHTP over I2C: write data directly (no register address)
    // xfer_pending=FALSE means SEND STOP condition (matches Arduino stop=true)
    OPERATE_RET ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, 
                                           (uint8_t*)buffer, len, FALSE);
    
    if (ret != OPRT_OK) {
        PR_DEBUG("BNO08x: i2c_write_bytes - I2C send failed (ret=%d, addr=0x%02X, len=%u)", 
                 ret, dev->i2c_addr, len);
        return false;
    }
    
    PR_DEBUG("BNO08x: i2c_write_bytes - wrote %u bytes to 0x%02X", len, dev->i2c_addr);
    return true;
}

/**
 * @brief Read bytes via I2C - follows Arduino Wire.requestFrom() pattern
 * Arduino: requestFrom(_deviceAddress, len, stop=true)
 * Tuya: tkl_i2c_master_receive(..., xfer_pending=FALSE) sends STOP
 */
static bool i2c_read_bytes(bno08x_dev_t *dev, uint8_t *buffer, size_t len)
{
    if (!dev || !buffer || len == 0) {
        PR_DEBUG("BNO08x: i2c_read_bytes - invalid parameters");
        return false;
    }
    
    // Clear buffer before reading
    memset(buffer, 0, len);
    
    // SHTP over I2C: read data directly (no register address)
    // xfer_pending=FALSE means SEND STOP condition (matches Arduino stop=true)
    OPERATE_RET ret = tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, 
                                              buffer, len, FALSE);
    
    if (ret != OPRT_OK) {
        PR_DEBUG("BNO08x: i2c_read_bytes - I2C receive failed (ret=%d, addr=0x%02X, len=%u)", 
                 ret, dev->i2c_addr, len);
        return false;
    }
    
    PR_DEBUG("BNO08x: i2c_read_bytes - read %u bytes from 0x%02X", len, dev->i2c_addr);
    return true;
}

/**
 * @brief Wait for INT pin to go low (indicates sensor ready)
 * Follows Arduino library pattern: 500ms timeout with hardware reset on failure
 */
static bool wait_for_int(bno08x_dev_t *dev)
{
    // If INT pin not configured, skip waiting (polling mode)
    if (!dev || dev->int_pin < 0) {
        return true;
    }
    
    // Wait up to 500ms for INT pin to go LOW (sensor ready)
    for (int i = 0; i < 500; i++) {
        TUYA_GPIO_LEVEL_E level;
        OPERATE_RET ret = tkl_gpio_read(dev->int_pin, &level);
        
        if (ret == OPRT_OK) {
            if (level == TUYA_GPIO_LEVEL_LOW) {
                // Sensor is ready (INT pin is LOW)
                return true;
            }
        } else {
            PR_DEBUG("BNO08x: GPIO read failed on INT pin (ret=%d), continuing", ret);
        }
        
        tal_system_sleep(1);
    }
    
    // Timeout - sensor not responding
    PR_WARN("BNO08x: Timeout waiting for INT pin (GPIO%d) - sensor not ready", dev->int_pin);
    
    // Perform hardware reset to try to recover (like Arduino library)
    if (dev->rst_pin >= 0) {
        PR_WARN("BNO08x: Attempting hardware reset to recover");
        hardware_reset_impl(dev);
    }
    
    return false;
}

/**
 * @brief Perform hardware reset
 * Follows Arduino library pattern: HIGH -> LOW (10ms) -> HIGH (10ms)
 */
static void hardware_reset_impl(bno08x_dev_t *dev)
{
    if (!dev || dev->rst_pin < 0) {
        PR_DEBUG("BNO08x: Hardware reset skipped (no RST pin configured)");
        return;
    }
    
    PR_DEBUG("BNO08x: Performing hardware reset on GPIO%d", dev->rst_pin);
    
    // Ensure pin is configured as OUTPUT (in case not already done)
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_OUTPUT,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_HIGH
    };
    tkl_gpio_init(dev->rst_pin, &gpio_cfg);
    
    // Reset sequence: HIGH -> LOW -> HIGH (matches Arduino library)
    tkl_gpio_write(dev->rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(10);
    tkl_gpio_write(dev->rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(10);
    tkl_gpio_write(dev->rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(10);
    
    PR_DEBUG("BNO08x: Hardware reset complete");
}

