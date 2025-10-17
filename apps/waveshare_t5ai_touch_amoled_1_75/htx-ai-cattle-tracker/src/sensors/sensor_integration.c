/**
 * @file sensor_integration.c
 * @brief Integration wrapper for BMM150, GPS, and Encoder sensors
 *
 * This source provides unified implementation for the BMM150 magnetometer, LC76G GPS module,
 * and rotary encoder input.
 *
 * GPS Interface Configuration:
 * ==========================
 * The GPS interface can be configured through Kconfig options:
 *
 * 1. I2C Interface (default):
 *    - CONFIG_USE_GPS_I2C=y
 *    - Uses I2C addresses: WR=0x50, RD=0x54
 *    - GPIO pins: SCL=20, SDA=21 (shared with touch display)
 *
 * 2. UART Interface:
 *    - CONFIG_USE_GPS_UART=y
 *    - Uses UART port 2, baudrate 115200
 *    - GPIO pins: TX=41, RX=40
 *
 * 3. NMEA Debug Logging (optional):
 *    - Enable via Kconfig: CONFIG_LC76G_ENABLE_NMEA_LOGS=y
 *    - Results in: LC76G_ENABLE_NMEA_LOGS=1 (defined in tuya_kconfig.h)
 *    - Enables detailed NMEA sentence parsing logs
 *    - Shows individual field values for debugging
 *
 * Configure via menuconfig:
 * make menuconfig -> Application config -> GPS Interface Type
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "sensor_integration.h"
#include "bno08x.h"
#include "lc76g.h"
#include "dev_config.h"

#ifdef ENABLE_ENCODER_INPUT
#include "drv_encoder.h"
#endif

#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)
#include "cattle_ai_tracker_app.h"
#include "ui_display.h"
#endif

#include "app_dp.h"

#if defined(ENABLE_CLOUD_API) && (ENABLE_CLOUD_API == 1)
#include "cloud_api.h"
#endif

#include "tal_log.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_mutex.h"
#include <math.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TASK_BNO08X_PRIORITY     THREAD_PRIO_2
#define TASK_BNO08X_SIZE         4096
#define TASK_GPS_PRIORITY        THREAD_PRIO_2
#define TASK_GPS_SIZE            4096
#define TASK_ENCODER_PRIORITY    THREAD_PRIO_2
#define TASK_ENCODER_SIZE        2048
#define ENCODER_POLL_INTERVAL_MS 100 /* Poll encoder every 100ms */

// BNO08x I2C Configuration - uses SAME I2C port and pins as BMM150 (Port 2, GPIO 14/15)
#define BNO08X_I2C_PORT          TUYA_I2C_NUM_2  // Same as BMM150
#define BNO08X_I2C_ADDR          BNO08X_DEFAULT_ADDRESS  // 0x4B (or 0x4A if ADR jumper closed)
#define BNO08X_INT_PIN           -1  // Optional interrupt pin (-1 to disable)
#define BNO08X_RST_PIN           -1  // Optional reset pin (-1 to disable)

#ifdef ENABLE_ENCODER_INPUT
#define ENCODER_STEPS_PER_ZOOM 2 /* Number of encoder steps per zoom level change */
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

#ifdef ENABLE_BNO08X_SENSOR
static THREAD_HANDLE sg_bno08x_handle = NULL;
static bno08x_dev_t g_bno08x_dev;
static compass_accuracy_cb_t sg_accuracy_callback = NULL;
#endif

#ifdef ENABLE_GPS_LC76G
static THREAD_HANDLE sg_gps_handle = NULL;
static lc76g_dev_t g_gps_dev;

// GPS interface configuration based on Kconfig
#if defined(CONFIG_USE_GPS_I2C) && defined(CONFIG_USE_GPS_UART)
#error "Both CONFIG_USE_GPS_I2C and CONFIG_USE_GPS_UART cannot be defined simultaneously"
#elif defined(CONFIG_USE_GPS_I2C)
// I2C interface configuration
#define GPS_INTERFACE_TYPE LC76G_INTERFACE_I2C
#elif defined(CONFIG_USE_GPS_UART)
// UART interface configuration
#define GPS_INTERFACE_TYPE LC76G_INTERFACE_UART
#ifndef CONFIG_GPS_UART_PORT
#define CONFIG_GPS_UART_PORT 2 // Default UART port
#endif
#ifndef CONFIG_GPS_UART_BAUDRATE
#define CONFIG_GPS_UART_BAUDRATE 115200 // LC76G default baudrate
#endif
#endif
#endif

#ifdef ENABLE_ENCODER_INPUT
static THREAD_HANDLE sg_encoder_handle = NULL;

/* Zoom control variables */
#ifdef ENABLE_GUI_TRACKER
/* Predefined zoom levels in meters */
static const int ZOOM_LEVELS[] = {
    50,    /* Level 0: 50m */
    100,   /* Level 1: 100m */
    200,   /* Level 2: 200m */
    500,   /* Level 3: 500m */
    1000,  /* Level 4: 1km */
    3000,  /* Level 5: 3km */
    5000,  /* Level 6: 5km */
    10000, /* Level 7: 10km */
    20000  /* Level 8: 20km */
};
#define ZOOM_LEVEL_COUNT (sizeof(ZOOM_LEVELS) / sizeof(ZOOM_LEVELS[0]))
static int sg_current_zoom_index = 2; /* Start at 200m (index 2) */
static int sg_accumulated_steps = 0;
#endif
#endif

static sensor_data_t g_sensor_data = {0};
static MUTEX_HANDLE g_sensor_mutex = NULL;

// I2C bus coordination mutex for shared I2C Port 0 (GPS + Touch)
MUTEX_HANDLE g_i2c_bus_mutex = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

// /**
//  * @brief Initialize I2C bus coordination mutex
//  */
// static OPERATE_RET __i2c_bus_mutex_init(void)
// {
//     if (g_i2c_bus_mutex == NULL) {
//         OPERATE_RET ret = tal_mutex_create_init(&g_i2c_bus_mutex);
//         if (ret != OPRT_OK) {
//             PR_ERR("[I2C] Failed to create I2C bus mutex (error: %d)", ret);
//             return ret;
//         }
//         PR_INFO("[I2C] I2C bus coordination mutex created");
//     }
//     return OPRT_OK;
// }

#ifdef ENABLE_BNO08X_SENSOR

/**
 * @brief BNO08x sensor reading task - simple rotation vector reading
 * 
 * Follows Arduino demo pattern: init -> enable rotation vector -> read yaw in loop
 */
static void __bno08x_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;

    PR_INFO("[BNO08x] Task started - initializing sensor...");

    // Initialize BNO08x sensor (pinmux and I2C setup handled internally)
    rt = bno08x_init(&g_bno08x_dev, BNO08X_I2C_PORT, BNO08X_I2C_ADDR, 
                     BNO08X_INT_PIN, BNO08X_RST_PIN);
    if (rt != OPRT_OK) {
        PR_ERR("[BNO08x] Failed to initialize sensor (error: %d)", rt);
        tal_thread_delete(NULL);
        return;
    }

    PR_INFO("[BNO08x] Initialized successfully!");

    // Enable rotation vector report at 10Hz (100ms interval)
    rt = bno08x_enable_rotation_vector(&g_bno08x_dev, 100);
    if (rt != OPRT_OK) {
        PR_ERR("[BNO08x] Failed to enable rotation vector (error: %d)", rt);
        tal_thread_delete(NULL);
        return;
    }

    PR_INFO("[BNO08x] Rotation vector enabled at 10 Hz");

    // Enable dynamic calibration for all sensors
    rt = bno08x_set_calibration_config(&g_bno08x_dev, SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
    if (rt != OPRT_OK) {
        PR_WARN("[BNO08x] Failed to set calibration config (error: %d)", rt);
    } else {
        PR_INFO("[BNO08x] Dynamic calibration enabled");
    }

    // Update sensor status
    tal_mutex_lock(g_sensor_mutex);
    g_sensor_data.bno08x_ready = true;
    tal_mutex_unlock(g_sensor_mutex);

    // Main reading loop - simple pattern like Arduino demo
    uint8_t last_accuracy = 0;
    while (1) {
        // Check if sensor was reset
        if (bno08x_was_reset(&g_bno08x_dev)) {
            PR_WARN("[BNO08x] Sensor reset detected - re-enabling rotation vector");
            bno08x_enable_rotation_vector(&g_bno08x_dev, 100);
        }

        // Get sensor event (calls sh2_service internally - like Arduino)
        if (bno08x_get_sensor_event(&g_bno08x_dev)) {
            // Check if this is a rotation vector event
            if (bno08x_get_sensor_event_id(&g_bno08x_dev) == BNO08X_ROTATION_VECTOR) {
                // Get heading (yaw) in degrees (0-360) - like Arduino getYaw()
                float heading_degrees = bno08x_get_yaw_degrees(&g_bno08x_dev);
                float pitch_degrees = bno08x_get_pitch_degrees(&g_bno08x_dev);
                float roll_degrees = bno08x_get_roll_degrees(&g_bno08x_dev);

                // Get quaternion components
                float quat_i = bno08x_get_quat_i(&g_bno08x_dev);
                float quat_j = bno08x_get_quat_j(&g_bno08x_dev);
                float quat_k = bno08x_get_quat_k(&g_bno08x_dev);
                float quat_real = bno08x_get_quat_real(&g_bno08x_dev);

                // Get accuracy status (BNO08x auto-calibrates)
                uint8_t accuracy = bno08x_get_quat_accuracy(&g_bno08x_dev);

                // Optional: notify about accuracy changes
                if (accuracy != last_accuracy) {
                    PR_INFO("[BNO08x] Accuracy changed: %d (%s)", accuracy,
                            accuracy == 0 ? "Unreliable" :
                            accuracy == 1 ? "Low" :
                            accuracy == 2 ? "Medium" : "High");
                    last_accuracy = accuracy;
                    
                    if (sg_accuracy_callback != NULL) {
                        sg_accuracy_callback(accuracy);
                    }
                }

                // Update global sensor data
                tal_mutex_lock(g_sensor_mutex);
                g_sensor_data.heading_degrees = heading_degrees;
                g_sensor_data.pitch_degrees = pitch_degrees;
                g_sensor_data.roll_degrees = roll_degrees;
                g_sensor_data.quat_i = quat_i;
                g_sensor_data.quat_j = quat_j;
                g_sensor_data.quat_k = quat_k;
                g_sensor_data.quat_real = quat_real;
                g_sensor_data.quat_accuracy = accuracy;
                tal_mutex_unlock(g_sensor_mutex);

                PR_INFO("!!!!!!!!! BNO08x: heading=%.1f° pitch=%.1f° roll=%.1f° accuracy=%d", 
                        heading_degrees, pitch_degrees, roll_degrees, accuracy);

#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)
                // Update tracker compass with BNO08x heading data
                tracker_update_compass_heading(heading_degrees);
#endif

                // Debug output (can be enabled for testing)
                // PR_DEBUG("[BNO08x] Heading: %.1f°, Pitch: %.1f°, Roll: %.1f°, Acc: %d",
                //          heading_degrees, pitch_degrees, roll_degrees, accuracy);
            }
        }

        tal_system_sleep(10); // Fast loop like Arduino demo
    }
}
#endif

#ifdef ENABLE_ENCODER_INPUT
/**
 * @brief Rotary encoder monitoring task
 */
static void __encoder_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;
    int32_t last_angle = 0;
    uint8_t last_button_state = 0;

    PR_INFO("[ENCODER] Task started - initializing encoder...");
    PR_INFO("[ENCODER] GPIO Configuration:");
    PR_INFO("[ENCODER] - Input A Pin (clockwise):        GPIO %d", DECODER_INPUT_A);
    PR_INFO("[ENCODER] - Input B Pin (counter-clockwise): GPIO %d", DECODER_INPUT_B);
    PR_INFO("[ENCODER] - Button Press Pin:                GPIO %d", DECODER_INPUT_P);

    // Initialize encoder driver
    rt = tkl_encoder_init();
    if (rt != OPRT_OK) {
        PR_ERR("[ENCODER] Failed to initialize encoder driver (error: %d)", rt);
        tal_thread_delete(NULL);
        return;
    }

    PR_INFO("[ENCODER] Initialized successfully!");

#ifdef ENABLE_GUI_TRACKER
    PR_INFO("[ENCODER] Zoom control enabled - steps per zoom: %d", ENCODER_STEPS_PER_ZOOM);
    PR_INFO("[ENCODER] Initial zoom level: %dm (index %d)", ZOOM_LEVELS[sg_current_zoom_index], sg_current_zoom_index);
#endif

    // Update sensor status
    tal_mutex_lock(g_sensor_mutex);
    g_sensor_data.encoder_ready = true;
    last_angle = encoder_get_angle();
    g_sensor_data.encoder_angle = last_angle;
    tal_mutex_unlock(g_sensor_mutex);

    PR_INFO("[ENCODER] Initial angle: %d", last_angle);

    // Main monitoring loop
    while (1) {
        int32_t current_angle = encoder_get_angle();
        uint8_t button_pressed = encoder_get_pressed();

        // Check for angle changes
        if (current_angle != last_angle) {
            int32_t angle_delta = current_angle - last_angle;

            // Update global sensor data
            tal_mutex_lock(g_sensor_mutex);
            g_sensor_data.encoder_angle = current_angle;
            tal_mutex_unlock(g_sensor_mutex);

            if (angle_delta > 0) {
                PR_DEBUG("[ENCODER] Rotated clockwise: angle = %d (delta: +%d)", current_angle, angle_delta);
            } else {
                PR_DEBUG("[ENCODER] Rotated counter-clockwise: angle = %d (delta: %d)", current_angle, angle_delta);
            }

#ifdef ENABLE_GUI_TRACKER
            // Handle UI zoom control
            sg_accumulated_steps += angle_delta;

            // Check if we've accumulated enough steps to change zoom level
            if (sg_accumulated_steps >= ENCODER_STEPS_PER_ZOOM) {
                // Zoom out (increase scale)
                int steps = sg_accumulated_steps / ENCODER_STEPS_PER_ZOOM;
                sg_accumulated_steps = sg_accumulated_steps % ENCODER_STEPS_PER_ZOOM;

                if (sg_current_zoom_index + steps < (int)ZOOM_LEVEL_COUNT) {
                    sg_current_zoom_index += steps;
                    animate_distance_scale(ZOOM_LEVELS[sg_current_zoom_index]);
                    PR_INFO("[ENCODER] Zoom OUT to %dm (index %d)", ZOOM_LEVELS[sg_current_zoom_index],
                            sg_current_zoom_index);
                } else {
                    // Clamp to max zoom level
                    sg_current_zoom_index = ZOOM_LEVEL_COUNT - 1;
                    sg_accumulated_steps = 0;
                    PR_DEBUG("[ENCODER] Already at maximum zoom level");
                }
            } else if (sg_accumulated_steps <= -ENCODER_STEPS_PER_ZOOM) {
                // Zoom in (decrease scale)
                int steps = (-sg_accumulated_steps) / ENCODER_STEPS_PER_ZOOM;
                sg_accumulated_steps = -((-sg_accumulated_steps) % ENCODER_STEPS_PER_ZOOM);

                if (sg_current_zoom_index - steps >= 0) {
                    sg_current_zoom_index -= steps;
                    animate_distance_scale(ZOOM_LEVELS[sg_current_zoom_index]);
                    PR_INFO("[ENCODER] Zoom IN to %dm (index %d)", ZOOM_LEVELS[sg_current_zoom_index],
                            sg_current_zoom_index);
                } else {
                    // Clamp to min zoom level
                    sg_current_zoom_index = 0;
                    sg_accumulated_steps = 0;
                    PR_DEBUG("[ENCODER] Already at minimum zoom level");
                }
            }
#endif

            last_angle = current_angle;
        }

        // Check button state changes
        if (button_pressed && !last_button_state) {
            tal_mutex_lock(g_sensor_mutex);
            g_sensor_data.encoder_button = true;
            tal_mutex_unlock(g_sensor_mutex);

            PR_INFO("[ENCODER] Button pressed! Current angle: %d", current_angle);

#ifdef ENABLE_GUI_TRACKER
            // Reset to default zoom (200m)
            sg_current_zoom_index = 2;
            sg_accumulated_steps = 0;
            animate_distance_scale(ZOOM_LEVELS[sg_current_zoom_index]);
            PR_INFO("[ENCODER] Button pressed - reset to default zoom: %dm", ZOOM_LEVELS[sg_current_zoom_index]);
#endif

            last_button_state = 1;
        } else if (!button_pressed && last_button_state) {
            tal_mutex_lock(g_sensor_mutex);
            g_sensor_data.encoder_button = false;
            tal_mutex_unlock(g_sensor_mutex);

            PR_INFO("[ENCODER] Button released");
            last_button_state = 0;
        }

        // Sleep for polling interval
        tal_system_sleep(ENCODER_POLL_INTERVAL_MS);
    }
}
#endif

/**
 * @brief GPS sensor reading task
 */
#ifdef ENABLE_GPS_LC76G
__attribute__((unused)) static void __gps_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;

    PR_INFO("[GPS] Task started - initializing GPS...");

    // Small delay to ensure system is fully initialized
    tal_system_sleep(500);

    // Initialize GPS module based on configured interface
#if defined(CONFIG_USE_GPS_I2C)
    // I2C Interface Configuration
    PR_INFO("[GPS] Using I2C Interface");
    PR_INFO("[GPS] I2C Port 0 (GPIO %d/%d) - shared with touch display", GPS_I2C_SCL_PIN, GPS_I2C_SDA_PIN);
    PR_INFO("[GPS] I2C addresses: WR=0x%02X, RD=0x%02X", LC76G_ADDRESS, DEVICE_ADDRESS_R);

    rt = dev_i2c_init();
    if (rt != OPRT_OK) {
        PR_ERR("[GPS] Failed to initialize I2C Port 0 (error: %d)", rt);
        PR_ERR("[GPS] Check if I2C pins are correct");
        tal_thread_delete(NULL);
        return;
    }
    PR_INFO("[GPS] I2C Port 0 initialized successfully");

    // Initialize GPS with I2C interface
    rt = lc76g_init_i2c(&g_gps_dev, LC76G_ADDRESS, DEVICE_ADDRESS_R);
    if (rt != OPRT_OK) {
        PR_ERR("[GPS] Failed to initialize GPS I2C interface (error: %d)", rt);
        tal_thread_delete(NULL);
        return;
    }
    PR_INFO("[GPS] LC76G I2C interface initialized successfully");

#elif defined(CONFIG_USE_GPS_UART)
    // UART Interface Configuration
    PR_INFO("[GPS] Using UART Interface");
    PR_INFO("[GPS] UART Port: %d, Baudrate: %d", CONFIG_GPS_UART_PORT, CONFIG_GPS_UART_BAUDRATE);
#ifdef CONFIG_GPS_UART_TX_PIN
    PR_INFO("[GPS] TX Pin: GPIO %d, RX Pin: GPIO %d", CONFIG_GPS_UART_TX_PIN, CONFIG_GPS_UART_RX_PIN);
#endif

    // Initialize GPS with UART interface
    rt = lc76g_init_uart(&g_gps_dev, (TUYA_UART_NUM_E)CONFIG_GPS_UART_PORT, CONFIG_GPS_UART_BAUDRATE);
    if (rt != OPRT_OK) {
        PR_ERR("[GPS] Failed to initialize GPS UART interface (error: %d)", rt);
        tal_thread_delete(NULL);
        return;
    }
    PR_INFO("[GPS] LC76G UART interface initialized successfully");

#else
#error "Unknown GPS interface type"
#endif

    PR_INFO("[GPS] GPS module initialized successfully!");

    // Update sensor status
    tal_mutex_lock(g_sensor_mutex);
    g_sensor_data.gps_ready = true;
    tal_mutex_unlock(g_sensor_mutex);

    // Main reading loop - simple single shot read
    int error_count = 0;
    int success_count = 0;
    while (1) {
        // Single attempt GPS read
        OPERATE_RET read_ret = lc76g_get_data(&g_gps_dev);

        if (read_ret != OPRT_OK) {
            error_count++;
            PR_ERR("[GPS] Failed to read GPS data (error: %d, count: %d)", read_ret, error_count);
            if (error_count == 1 || error_count % 10 == 0) {
#if defined(CONFIG_USE_GPS_I2C)
                PR_ERR("[GPS] Check I2C connection and GPS module");
                PR_ERR("[GPS] I2C addresses: WR=0x%02X, RD=0x%02X", LC76G_ADDRESS, DEVICE_ADDRESS_R);
                PR_ERR("[GPS] I2C Port 0 pins: SCL=%d, SDA=%d", GPS_I2C_SCL_PIN, GPS_I2C_SDA_PIN);
                PR_ERR("[GPS] Note: I2C bus is shared with touch display");
#elif defined(CONFIG_USE_GPS_UART)
                PR_ERR("[GPS] Check UART connection and GPS module");
                PR_ERR("[GPS] UART Port: %d, Baudrate: %d", CONFIG_GPS_UART_PORT, CONFIG_GPS_UART_BAUDRATE);
#ifdef CONFIG_GPS_UART_TX_PIN
                PR_ERR("[GPS] TX Pin: GPIO %d, RX Pin: GPIO %d", CONFIG_GPS_UART_TX_PIN, CONFIG_GPS_UART_RX_PIN);
#endif
                PR_ERR("[GPS] Note: Verify UART wiring and baudrate settings");
#endif
            }
            tal_system_sleep(5000); // Wait longer on error
            continue;
        }

        // Success! Reset error count and increment success count
        if (error_count > 0) {
            PR_INFO("[GPS] GPS data read recovered after %d errors", error_count);
            error_count = 0;
        }
        success_count++;

        const lc76g_state_t *s = lc76g_get_state();

        // Update global sensor data
        tal_mutex_lock(g_sensor_mutex);
        g_sensor_data.latitude_deg = s->latitude_deg;
        g_sensor_data.longitude_deg = s->longitude_deg;
        g_sensor_data.altitude_m = s->altitude_m;
        g_sensor_data.speed_kmh = s->speed_kmh;
        g_sensor_data.course_deg = s->course_deg;
        g_sensor_data.satellites_in_use = s->satellites_in_use;
        g_sensor_data.fix_quality = s->fix_quality;
        tal_mutex_unlock(g_sensor_mutex);

        // Print to console with clear status indicators
        char datebuf[7] = {0};
        lc76g_get_data_ddmmyy(datebuf);

#if defined(LC76G_ENABLE_NMEA_LOGS) && (LC76G_ENABLE_NMEA_LOGS == 1)
        const char *fix_status = (s->fix_quality > 0) ? "FIX" : "SEARCH";
        const char *data_valid = (s->connect_state > 0) ? "VALID" : "INVALID";

        PR_INFO("GPS Status: %s/%s", fix_status, data_valid);
        PR_INFO("UTC Time: %02d:%02d:%02d.%03dZ", s->utc_hour, s->utc_minute, s->utc_second, s->utc_millisecond);
        PR_INFO("Position: %.6f, %.6f", s->latitude_deg, s->longitude_deg);
        PR_INFO("Altitude: %.1fm | Satellites: %d | Signal: %d/5 | Speed: %.1fkm/h", s->altitude_m,
                s->satellites_in_use, s->signal_level_5, s->speed_kmh);
        // Log success and sleep for 5 seconds
        PR_INFO("[GPS] Success #%d - sleeping for 5 seconds", success_count);
#endif

        app_gps_position_upload(g_sensor_data.latitude_deg, g_sensor_data.longitude_deg);

#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)
        // Update UI with new GPS self position
        if (s->fix_quality > 0) {
            ui_update_self_gps_position(s->latitude_deg, s->longitude_deg, s->satellites_in_use);
        }
#endif

#if defined(ENABLE_CLOUD_API) && (ENABLE_CLOUD_API == 1)
        // get cloud cattle position
        cattle_location_t loc = {0};
        OPERATE_RET cloud_ret = cloud_api_get_cattle_location(&loc);
        
        // Update UI with cattle location if successful
        if (cloud_ret == OPRT_OK && (loc.lat != 0.0 || loc.lon != 0.0)) {
#if defined(ENABLE_GUI_TRACKER) && (ENABLE_GUI_TRACKER == 1)
            ui_add_cattle_marker(loc.lat, loc.lon, loc.cattleId);
#endif
            PR_INFO("[CATTLE] Location received: lat=%.6f, lon=%.6f, id=%s", 
                    loc.lat, loc.lon, loc.cattleId);
        }
#endif
        // ui
        gps_set_tracker_position(g_sensor_data.latitude_deg, g_sensor_data.longitude_deg);

        tal_system_sleep(5000); // Sleep for 5 seconds on success
    }
}
#endif
/**
 * @brief Initialize BNO08x IMU sensor
 */
OPERATE_RET sensor_bno08x_init(void)
{
    PR_INFO("[SENSOR] Initializing BNO08x IMU sensor...");

    // Create mutex if not exists
    if (g_sensor_mutex == NULL) {
        OPERATE_RET ret = tal_mutex_create_init(&g_sensor_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create mutex (error: %d)", ret);
            return ret;
        }
    }

    // Note: System initialization (GPIO, buttons) is done by board_register_hardware()
    // in the main app, so we don't call dev_sys_init() here to avoid conflicts
    PR_INFO("[SENSOR] BNO08x init ready (system already initialized by main app)");

    return OPRT_OK;
}

/**
 * @brief Initialize LC76G GPS module
 */
OPERATE_RET sensor_gps_init(void)
{
    PR_INFO("[SENSOR] Initializing LC76G GPS...");

    // Create mutex if not exists
    if (g_sensor_mutex == NULL) {
        OPERATE_RET ret = tal_mutex_create_init(&g_sensor_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create mutex (error: %d)", ret);
            return ret;
        }
    }

    // Note: System initialization (GPIO, buttons) is done by board_register_hardware()
    // in the main app, so we don't call dev_sys_init() here to avoid conflicts
    PR_INFO("[SENSOR] GPS init ready (system already initialized by main app)");

    return OPRT_OK;
}

/**
 * @brief Initialize rotary encoder input
 */
OPERATE_RET sensor_encoder_init(void)
{
    PR_INFO("[SENSOR] Initializing rotary encoder...");

    // Create mutex if not exists
    if (g_sensor_mutex == NULL) {
        OPERATE_RET ret = tal_mutex_create_init(&g_sensor_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create mutex (error: %d)", ret);
            return ret;
        }
    }

    // Note: Encoder initialization is done in the encoder task thread
    // to avoid blocking the main initialization
    PR_INFO("[SENSOR] Encoder init ready (will initialize in task thread)");

    return OPRT_OK;
}

/**
 * @brief Start sensor reading tasks
 */
OPERATE_RET sensor_tasks_start(void)
{
    OPERATE_RET ret = OPRT_OK;

    PR_INFO("[SENSOR] Starting sensor tasks...");

// Log GPS interface configuration
#ifdef ENABLE_GPS_LC76G
#if defined(CONFIG_USE_GPS_I2C)
    PR_INFO("[SENSOR] GPS configured for I2C interface");
#elif defined(CONFIG_USE_GPS_UART)
    PR_INFO("[SENSOR] GPS configured for UART interface");
#else
    PR_ERR("[SENSOR] GPS interface type not recognized!");
#endif

// Log NMEA debugging status
#if LC76G_ENABLE_NMEA_LOGS
    PR_INFO("[SENSOR] GPS NMEA detailed logging: ENABLED");
#else
    PR_INFO("[SENSOR] GPS NMEA detailed logging: disabled");
#endif
#endif

// Start BNO08x task
#ifdef ENABLE_BNO08X_SENSOR
    if (sg_bno08x_handle == NULL) {
        static THREAD_CFG_T bno08x_param = {
            .priority = TASK_BNO08X_PRIORITY, .stackDepth = TASK_BNO08X_SIZE, .thrdname = "bno08x"};
        ret = tal_thread_create_and_start(&sg_bno08x_handle, NULL, NULL, __bno08x_task, NULL, &bno08x_param);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create BNO08x task (error: %d)", ret);
            return ret;
        }
        PR_INFO("[SENSOR] BNO08x task started");
    }
#endif

// Start GPS task
#ifdef ENABLE_GPS_LC76G
    if (sg_gps_handle == NULL) {
        static THREAD_CFG_T gps_param = {.priority = TASK_GPS_PRIORITY - 3, // Much higher priority than touch display
                                         .stackDepth = TASK_GPS_SIZE,
                                         .thrdname = "gps"};
        ret = tal_thread_create_and_start(&sg_gps_handle, NULL, NULL, __gps_task, NULL, &gps_param);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create GPS task (error: %d)", ret);
            return ret;
        }
        PR_INFO("[SENSOR] GPS task started with higher priority");
    }
#endif

// Start Encoder task
#ifdef ENABLE_ENCODER_INPUT
    if (sg_encoder_handle == NULL) {
        static THREAD_CFG_T encoder_param = {
            .priority = TASK_ENCODER_PRIORITY, .stackDepth = TASK_ENCODER_SIZE, .thrdname = "encoder"};
        ret = tal_thread_create_and_start(&sg_encoder_handle, NULL, NULL, __encoder_task, NULL, &encoder_param);
        if (ret != OPRT_OK) {
            PR_ERR("[SENSOR] Failed to create encoder task (error: %d)", ret);
            return ret;
        }
        PR_INFO("[SENSOR] Encoder task started");
    }
#endif

    return OPRT_OK;
}

/**
 * @brief Get current sensor data
 */
OPERATE_RET sensor_get_data(sensor_data_t *data)
{
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (g_sensor_mutex == NULL) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(g_sensor_mutex);
    memcpy(data, &g_sensor_data, sizeof(sensor_data_t));
    tal_mutex_unlock(g_sensor_mutex);

    return OPRT_OK;
}

/**
 * @brief Print sensor readings to console
 */
void sensor_print_readings(void)
{
    sensor_data_t data;
    if (sensor_get_data(&data) == OPRT_OK) {
        PR_INFO("=== Sensor Readings ===");

#ifdef ENABLE_BNO08X_SENSOR
        PR_INFO("BNO08x: heading=%.1f° pitch=%.1f° roll=%.1f° accuracy=%d ready=%d", 
                data.heading_degrees, data.pitch_degrees, data.roll_degrees,
                data.quat_accuracy, data.bno08x_ready);
#endif

#ifdef ENABLE_GPS_LC76G
        PR_INFO("GPS: lat=%.6f lon=%.6f alt=%.1fm sats=%d fix=%d ready=%d", data.latitude_deg, data.longitude_deg,
                data.altitude_m, data.satellites_in_use, data.fix_quality, data.gps_ready);
#endif

#ifdef ENABLE_ENCODER_INPUT
        PR_INFO("ENCODER: angle=%d button=%s ready=%d", data.encoder_angle,
                data.encoder_button ? "PRESSED" : "released", data.encoder_ready);
#endif
    }
}

/**
 * @brief Register a callback for compass accuracy monitoring (optional)
 */
OPERATE_RET sensor_bno08x_register_accuracy_cb(compass_accuracy_cb_t callback)
{
#ifdef ENABLE_BNO08X_SENSOR
    sg_accuracy_callback = callback;
    PR_INFO("[SENSOR] Compass accuracy callback %s", callback ? "registered" : "unregistered");
    return OPRT_OK;
#else
    PR_ERR("[SENSOR] BNO08x sensor not enabled");
    return OPRT_NOT_SUPPORTED;
#endif
}
