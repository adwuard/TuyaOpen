/**
 * @file tuya_main.c
 * @brief Implements main audio functionality for IoT device
 *
 * This source file provides the implementation of the main audio functionalities
 * required for an IoT device. It includes functionality for audio processing,
 * device initialization, event handling, and network communication. The
 * implementation supports audio volume control, data point processing, and
 * interaction with the Tuya IoT platform. This file is essential for developers
 * working on IoT applications that require audio capabilities and integration
 * with the Tuya IoT ecosystem.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include <assert.h>
#include "cJSON.h"
#include "tal_api.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "netmgr.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "tuya_authorize.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#include "app_display.h"
#include "ui_display.h"
#include "tuya_lvgl.h"
#endif

#include "board_com_api.h"

#include "app_chat_bot.h"
#include "ai_audio.h"
#include "reset_netcfg.h"
#include "app_system_info.h"
#include "app_dp.h"

#if defined(ENABLE_CLOUD_API) && (ENABLE_CLOUD_API == 1)
#include "cloud_api.h"
#endif

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
#include "app_battery.h"
#endif

#if defined(ENABLE_BNO08X_SENSOR) && (ENABLE_BNO08X_SENSOR == 1) || defined(ENABLE_GPS_LC76G) && (ENABLE_GPS_LC76G == 1)
#include "sensor_integration.h"
#endif

/* Tuya device handle */
tuya_iot_client_t ai_client;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define DPID_VOLUME 3

static uint8_t _need_reset = 0;

/**
 * @brief user defined log output api, in this demo, it will use uart0 as log-tx
 *
 * @param str log string
 * @return void
 */
void user_log_output_cb(const char *str)
{
    tal_uart_write(TUYA_UART_NUM_0, (const uint8_t *)str, strlen(str));
}

/**
 * @brief user defined upgrade notify callback, it will notify device a OTA request received
 *
 * @param client device info
 * @param upgrade the upgrade request info
 * @return void
 */
void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    PR_INFO("----- Upgrade information -----");
    PR_INFO("OTA Channel: %d", cJSON_GetObjectItem(upgrade, "type")->valueint);
    PR_INFO("Version: %s", cJSON_GetObjectItem(upgrade, "version")->valuestring);
    PR_INFO("Size: %s", cJSON_GetObjectItem(upgrade, "size")->valuestring);
    PR_INFO("MD5: %s", cJSON_GetObjectItem(upgrade, "md5")->valuestring);
    PR_INFO("HMAC: %s", cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    PR_INFO("URL: %s", cJSON_GetObjectItem(upgrade, "url")->valuestring);
    PR_INFO("HTTPS URL: %s", cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);
}

OPERATE_RET audio_dp_obj_proc(dp_obj_recv_t *dpobj)
{
    uint32_t index = 0;
    for (index = 0; index < dpobj->dpscnt; index++) {
        dp_obj_t *dp = dpobj->dps + index;
        PR_DEBUG("idx:%d dpid:%d type:%d ts:%u", index, dp->id, dp->type, dp->time_stamp);

        switch (dp->id) {
        case DPID_VOLUME: {
            uint8_t volume = dp->value.dp_value;
            PR_DEBUG("volume:%d", volume);
            ai_audio_set_volume(volume);
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            /* Update UI volume slider */
            ui_set_system_volume(volume);

            /* Show notification */
            char volume_str[20] = {0};
            snprintf(volume_str, sizeof(volume_str), "%s%d", VOLUME, volume);
            app_display_send_msg(TY_DISPLAY_TP_NOTIFICATION, (uint8_t *)volume_str, strlen(volume_str));
#endif
            break;
        }
        default:
            break;
        }
    }

    return OPRT_OK;
}

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
/**
 * @brief Volume change handler called when UI slider changes
 * @param volume Volume value from UI (0-100)
 */
static void volume_ui_change_handler(int volume)
{
    PR_INFO("Volume changed from UI: %d%%", volume);
    app_volume_set(volume);
}

/**
 * @brief Network link type change callback
 * @param data Event data (unused)
 * @return OPRT_OK
 *
 * This callback is triggered whenever the network connection type changes
 * (e.g., WiFi to Cellular, or connection goes up/down). It updates the
 * settings panel network icon to reflect the current active connection.
 */
static OPERATE_RET __network_link_type_change_cb(void *data)
{
    (void)data; /* Unused parameter */

    PR_INFO("Network link type changed - updating UI");

    /* Update network status icon in settings panel */
    ui_update_network_status();

    return OPRT_OK;
}
#endif

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
#include "tal_cellular.h"
#define TI_META_SAVE "tuya.device.meta.save"

OPERATE_RET httpc_put_iccid(char iccid[21])
{
    OPERATE_RET rt = OPRT_OK;
    int buffer_len = 72;
    char *post_data = tal_malloc(buffer_len);
    TUYA_CHECK_NULL_RETURN(post_data, OPRT_MALLOC_FAILED);

    memset(post_data, 0, buffer_len);

    TIME_T timestamp = 0;
    timestamp = tal_time_get_posix();

    snprintf(post_data, buffer_len, "{\"metas\":{\"catIccId\":\"%s\"},\"t\":\"%d\"}", iccid, timestamp);

    rt = atop_service_comm_post_simple(TI_META_SAVE, "1.0", post_data, NULL, NULL);
    tal_free(post_data);
    return rt;
}

OPERATE_RET cellular_http_upload_iccid(void)
{
    OPERATE_RET rt;
    static uint8_t is_cellular_ccid_reported = FALSE;
    char iccid[TAL_CELLULAR_CCID_LEN + 1] = {0};

    if (is_cellular_ccid_reported) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tal_cellular_get_ccid(iccid));

    if (strlen(iccid) == 0) {
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(httpc_put_iccid(iccid));

    is_cellular_ccid_reported = TRUE;
    PR_NOTICE("cellular report ccid %s to Tuya cloud", iccid);
    return rt;
}
#endif
/**
 * @brief user defined event handler
 *
 * @param client device info
 * @param event the event info
 * @return void
 */
void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    PR_DEBUG("Tuya Event ID:%d(%s)", event->id, EVENT_ID2STR(event->id));
    PR_INFO("Device Free heap %d", tal_system_get_free_heap_size());

    switch (event->id) {
    case TUYA_EVENT_BIND_START:
        PR_INFO("Device Bind Start!");
        if (_need_reset == 1) {
            PR_INFO("Device Reset!");
            tal_system_reset();
        }

        ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CFG);
        break;

    case TUYA_EVENT_BIND_TOKEN_ON:
        break;

    /* MQTT with tuya cloud is connected, device online */
    case TUYA_EVENT_MQTT_CONNECTED:
        PR_INFO("Device MQTT Connected!");
        tal_event_publish(EVENT_MQTT_CONNECTED, NULL);

        static uint8_t first = 1;
        if (first) {
            first = 0;

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            UI_WIFI_STATUS_E wifi_status = UI_WIFI_STATUS_GOOD;
            app_display_send_msg(TY_DISPLAY_TP_NETWORK, (uint8_t *)&wifi_status, sizeof(UI_WIFI_STATUS_E));
#endif

            ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CONNECTED);
            app_dp_update_all();
        }
        break;

    /* MQTT with tuya cloud is disconnected, device offline */
    case TUYA_EVENT_MQTT_DISCONNECT:
        PR_INFO("Device MQTT DisConnected!");
        tal_event_publish(EVENT_MQTT_DISCONNECTED, NULL);
        break;

    /* RECV upgrade request */
    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;

    /* Sync time with tuya Cloud */
    case TUYA_EVENT_TIMESTAMP_SYNC:
        PR_INFO("Sync timestamp:%d", event->value.asInteger);
        tal_time_set_posix(event->value.asInteger, 1);

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        /* Update UI with current local time and date */
        {
            TIME_T posix_time = tal_time_get_posix();
            POSIX_TM_S tm_time;
            OPERATE_RET ret = tal_time_get_local_time_custom(posix_time, &tm_time);
            if (ret == OPRT_OK) {
                tuya_lvgl_mutex_lock();
                /* Update time (HH:MM) */
                ui_set_settings_time(tm_time.tm_hour, tm_time.tm_min);

                /* Update date (YYYY/MM/DD) */
                ui_set_settings_date(tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday);
                tuya_lvgl_mutex_unlock();

                PR_INFO("UI time updated (local): %04d/%02d/%02d %02d:%02d", tm_time.tm_year + 1900, tm_time.tm_mon + 1,
                        tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min);
            }
        }
#endif

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
        cellular_http_upload_iccid();
#endif
        break;

    case TUYA_EVENT_RESET:
        PR_INFO("Device Reset:%d", event->value.asInteger);

        _need_reset = 1;
        break;

    /* RECV OBJ DP */
    case TUYA_EVENT_DP_RECEIVE_OBJ: {
        dp_obj_recv_t *dpobj = event->value.dpobj;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d CNT:%u", dpobj->cmd_tp, dpobj->dtt_tp, dpobj->dpscnt);
        if (dpobj->devid != NULL) {
            PR_DEBUG("devid.%s", dpobj->devid);
        }

        for (uint32_t i = 0; i < dpobj->dpscnt; i++) {
            app_dp_process(dpobj->dps[i].id, dpobj->dps[i].type, dpobj->dps[i].value);
        }
    } break;

    /* RECV RAW DP */
    case TUYA_EVENT_DP_RECEIVE_RAW: {
        dp_raw_recv_t *dpraw = event->value.dpraw;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d", dpraw->cmd_tp, dpraw->dtt_tp);
        if (dpraw->devid != NULL) {
            PR_DEBUG("devid.%s", dpraw->devid);
        }

        uint32_t index = 0;
        dp_raw_t *dp = &dpraw->dp;
        PR_DEBUG("dpid:%d type:RAW len:%d data:", dp->id, dp->len);
        for (index = 0; index < dp->len; index++) {
            PR_DEBUG_RAW("%02x", dp->data[index]);
        }

        tuya_iot_dp_raw_report(client, dpraw->devid, &dpraw->dp, 3);

    } break;

    default:
        break;
    }
}

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
#include "tkl_gpio.h"

static void __cellular_module_reset(void)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg;

    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    gpio_cfg.mode = TUYA_GPIO_PUSH_PULL;
    tkl_gpio_init(TUYA_GPIO_NUM_46, &gpio_cfg); // reset pin 24 is 1;
    tkl_gpio_write(TUYA_GPIO_NUM_46, TUYA_GPIO_LEVEL_HIGH);
    gpio_cfg.level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(TUYA_GPIO_NUM_45, &gpio_cfg); // power pin 9;
    tkl_gpio_write(TUYA_GPIO_NUM_45, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(200);                                 // delay 200ms
    tkl_gpio_write(TUYA_GPIO_NUM_45, TUYA_GPIO_LEVEL_LOW); // power on pin LOW
    tal_system_sleep(1200);                                // delay up 1s

    return;
}
#endif // ENABLE_CELLULAR

/**
 * @brief user defined network check callback, it will check the network every 1sec,
 *        in this demo it alwasy return ture due to it's a wired demo
 *
 * @return true
 * @return false
 */
bool user_network_check(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

void user_main(void)
{
    int ret = OPRT_OK;

    //! open iot development kit runtim init
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tal_time_service_init();
    tal_cli_init();
    tuya_authorize_init();

    reset_netconfig_start();

    tuya_iot_license_t license;

    if (OPRT_OK != tuya_authorize_read(&license)) {
        license.uuid = TUYA_OPENSDK_UUID;
        license.authkey = TUYA_OPENSDK_AUTHKEY;
        PR_WARN("Replace the TUYA_OPENSDK_UUID and TUYA_OPENSDK_AUTHKEY contents, otherwise the demo cannot work.\n \
                Visit https://platform.tuya.com/purchase/index?type=6 to get the open-sdk uuid and authkey.");
    }

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&ai_client, &(const tuya_iot_config_t){
                                        .software_ver = PROJECT_VERSION,
                                        .productkey = TUYA_PRODUCT_ID,
                                        .uuid = license.uuid,
                                        .authkey = license.authkey,
                                        // .firmware_key      = TUYA_DEVICE_FIRMWAREKEY,
                                        .event_handler = user_event_handler_on,
                                        .network_check = user_network_check,
                                    });
    assert(ret == OPRT_OK);

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    // network init
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    type |= NETCONN_CELLULAR;
    __cellular_module_reset();
#endif
    netmgr_init(type);
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE});
#endif

    PR_DEBUG("tuya_iot_init success");

    ret = board_register_hardware();
    if (ret != OPRT_OK) {
        PR_ERR("board_register_hardware failed");
    }

    ret = app_chat_bot_init();
    if (ret != OPRT_OK) {
        PR_ERR("tuya_audio_recorde_init failed");
    }

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    /* Register volume change handler to upload to cloud when UI slider changes */
    ui_register_volume_change_handler(volume_ui_change_handler);
    PR_INFO("Volume change handler registered");

    /* Subscribe to network link type change events for automatic UI updates */
    tal_event_subscribe(EVENT_LINK_TYPE_CHG, "ui_net", __network_link_type_change_cb, SUBSCRIBE_TYPE_NORMAL);
    PR_INFO("Network link type change event subscribed");
#endif

    app_system_info();

    /* Start tuya iot task */
    tuya_iot_start(&ai_client);

    tkl_wifi_set_lp_mode(0, 0);

    reset_netconfig_check();

#if defined(ENABLE_BNO08X_SENSOR) && (ENABLE_BNO08X_SENSOR == 1)
    PR_INFO("Initializing BNO08x IMU sensor...");
    ret = sensor_bno08x_init();
    if (ret != OPRT_OK) {
        PR_ERR("BNO08x initialization failed: %d", ret);
    } else {
        PR_INFO("BNO08x IMU sensor initialized successfully");
    }
#endif

#if defined(ENABLE_DEBUG_VIRTUAL_SIMULATION) && (ENABLE_DEBUG_VIRTUAL_SIMULATION == 1)
    PR_WARN("\r\n-------------------------------------------------");
    PR_WARN("-------------------------------------------------");
    PR_WARN("Virtual simulation mode enabled");
    PR_WARN("-------------------------------------------------");
    PR_WARN("-------------------------------------------------\r\n");
#endif

#if defined(ENABLE_GPS_LC76G) && (ENABLE_GPS_LC76G == 1)
    PR_INFO("Initializing GPS module...");
    ret = sensor_gps_init();
    if (ret != OPRT_OK) {
        PR_ERR("GPS initialization failed: %d", ret);
    } else {
        PR_INFO("GPS module initialized successfully");
    }
#endif

#if defined(ENABLE_BNO08X_SENSOR) && (ENABLE_BNO08X_SENSOR == 1) || defined(ENABLE_GPS_LC76G) && (ENABLE_GPS_LC76G == 1)
    PR_INFO("Starting sensor tasks...");
    ret = sensor_tasks_start();
    if (ret != OPRT_OK) {
        PR_ERR("Sensor tasks start failed: %d", ret);
    } else {
        PR_INFO("Sensor tasks started successfully");
        PR_INFO("BNO08x and GPS readings will be printed to console");
    }
#endif

#if defined(ENABLE_CLOUD_API) && (ENABLE_CLOUD_API == 1)
    cloud_api_init();
#endif

#if defined(ENABLE_BATTERY) && (ENABLE_BATTERY == 1)
    PR_INFO("Battery monitoring initialized");
    app_battery_init();
#endif

    for (;;) {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&ai_client);
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
