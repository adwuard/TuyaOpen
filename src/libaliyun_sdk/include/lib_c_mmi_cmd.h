/*
 * Copyright 2025 Alibaba Group Holding Ltd.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http: *www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __LIB_C_MMI_CMD__
#define __LIB_C_MMI_CMD__

#ifdef __cplusplus
extern "C" {
#endif

#include "c_utils/c_utils.h"
#include "lib_c_mmi_vl.h"

#define C_MMI_CMD_VALUE_UNDEFINED 0x7FFFFFFF

enum {
    C_MMI_CMD_BRIGHTNESS,  // 亮度设置
    C_MMI_CMD_VOLUME,      // 音量设置
    C_MMI_CMD_DEVICE,      // 设备控制
    C_MMI_CMD_SCREEN,      // 屏幕控制
    C_MMI_CMD_MULTIMEDIA,  // 多媒体控制
    C_MMI_CMD_APPLICATION, // 应用开关
    C_MMI_CMD_CAMERA,      // 拍照录像
    C_MMI_CMD_RECORDING,   // 录音
    C_MMI_CMD_MUSIC,       // 音乐
    C_MMI_CMD_TELEPHONE,   // 打电话
};

/**
 * @brief command事件回调函数类型
 *
 * @details 当command模块收到云端对应指令时触发的回调函数，用于处理指令事件
 *
 * @param event 事件类型，取值为加载某个指令组的指令事件枚举
 * @param param 事件参数，根据事件类型不同指向相对应的参数，没有参数时为NULL
 * @return int32_t 返回0表示处理成功，非0表示处理失败
 */
typedef int32_t (*command_user_callback)(uint32_t event, void *params);

/**
 * @brief 加载指定的指令模块组件
 *
 * @param cmd_module 指令模块类型
 * @param cb 回调函数，用户来实现，用于处理指令事件
 * @return int32_t 返回0表示处理成功，非0表示处理失败
 */
int32_t c_mmi_cmd_load(uint8_t cmd_module, command_user_callback cb);

enum { C_MMI_CMD_TYPE_UNKNOWN = 0, C_MMI_CMD_TYPE_SYSTEM, C_MMI_CMD_TYPE_MEDIA, C_MMI_CMD_TYPE_CALL };

/*************************** 音量设置组件开始 ***************************/
// event 音量设置相关命令的枚举定义
enum {
    C_MMI_CMD_VOLUME_UNKNOWN = 0,
    C_MMI_CMD_VOLUME_INCREASE,
    C_MMI_CMD_VOLUME_DECREASE,
    C_MMI_CMD_VOLUME_SET,
    C_MMI_CMD_VOLUME_MUTE,
    C_MMI_CMD_VOLUME_UNMUTE
};

// params 音量设置相关命令的参数定义
typedef struct {
    uint32_t value;
    uint32_t type;
} c_mmi_cmd_volume_param_t;
/*************************** 音量设置组件结束 ***************************/

/*************************** 亮度设置组件开始 ***************************/
// event 亮度设置相关命令的枚举定义
enum {
    C_MMI_CMD_BRIGHTNESS_UNKNOWN = 0,
    C_MMI_CMD_BRIGHTNESS_INCREASE,
    C_MMI_CMD_BRIGHTNESS_DECREASE,
    C_MMI_CMD_BRIGHTNESS_SET
};

// params 亮度设置相关命令的参数定义
typedef struct {
    uint32_t value;
} c_mmi_cmd_brightness_param_t;
/*************************** 亮度设置组件结束 ***************************/

/*************************** 音乐组件开始 ***************************/
#define C_MMI_CMD_MUSIC_LEN 128
// event 音乐相关命令的枚举定义
enum {
    C_MMI_CMD_MUSIC_UNKNOWN = 0,
    C_MMI_CMD_MUSIC_PLAY,                // 播放音乐
    C_MMI_CMD_MUSIC_PLAY_DAILY_PLAYLIST, // 播放每日推荐歌单
    C_MMI_CMD_MUSIC_PLAY_MY_COLLECTION,  // 播放我喜欢的歌单
    C_MMI_CMD_MUSIC_PLAY_RANDOMLY,       // 随机播放
    C_MMI_CMD_MUSIC_LIKE,                // 喜欢/收藏
    // C_MMI_CMD_MUSIC_UNLIKE                  // 不喜欢/取消收藏 仅多模态链路
};

/* mode：0-无、1-最新、2-最热 */
enum { C_MMI_CMD_MUSIC_SORT_UNKNOWN = 0, C_MMI_CMD_MUSIC_SORT_NEW, C_MMI_CMD_MUSIC_SORT_HOT };

/* mode：0-无、1-单曲循环、2-列表循环、3-心动模式 */
enum {
    C_MMI_CMD_MUSIC_MODE_UNKNOWN = 0,
    C_MMI_CMD_MUSIC_MODE_SINGLE_REPEAT,
    C_MMI_CMD_MUSIC_MODE_LIST_LOOP,
    C_MMI_CMD_MUSIC_MODE_HEART
};

// params 音乐相关命令的参数定义
typedef struct {
    char song[C_MMI_CMD_MUSIC_LEN];
    char artist[C_MMI_CMD_MUSIC_LEN];
    char album[C_MMI_CMD_MUSIC_LEN];
    char music_type[C_MMI_CMD_MUSIC_LEN];
    char style[C_MMI_CMD_MUSIC_LEN];
    char language[C_MMI_CMD_MUSIC_LEN];
    char general_tag[C_MMI_CMD_MUSIC_LEN];
    char era[C_MMI_CMD_MUSIC_LEN];
    uint32_t sort; // 枚举值  C_MMI_CMD_MUSIC_SORT_XXX
    uint32_t mode; // 枚举值  C_MMI_CMD_MUSIC_MODE_XXX
    char player[C_MMI_CMD_MUSIC_LEN];
} c_mmi_cmd_music_param_t;
/*************************** 音乐组件结束 ***************************/

/*************************** 设备控制组件开始 ***************************/
// event 设备控制相关命令的枚举定义
enum {
    C_MMI_CMD_DEVICE_UNKNOWN = 0,
    C_MMI_CMD_DEVICE_SHUTDOWN,
    C_MMI_CMD_DEVICE_QUIT,
    C_MMI_CMD_DEVICE_BACK,
    C_MMI_CMD_DEVICE_CONFIRM,
    C_MMI_CMD_DEVICE_CANCEL,
    C_MMI_CMD_DEVICE_SELECT,
    C_MMI_CMD_DEVICE_CHECK_BATTERY
};

// params 设备控制相关命令的参数定义
typedef struct {
    bool record;
} c_mmi_cmd_device_confirm_param_t;

typedef struct {
    uint32_t index;
} c_mmi_cmd_device_select_param_t;

/*************************** 设备控制组件结束 ***************************/

/*************************** 屏幕控制组件开始 ***************************/
// event 屏幕控制相关命令的枚举定义
enum {
    C_MMI_CMD_SCREEN_UNKNOWN = 0,
    C_MMI_CMD_SCREEN_OFF,
    C_MMI_CMD_SCREEN_SHOT,
    C_MMI_CMD_SCREEN_RECORDING,
    C_MMI_CMD_STOP_SCREEN_RECORDING
};

/*************************** 屏幕控制组件结束 ***************************/

/*************************** 多媒体控制组件开始 ***************************/
#define C_MMI_CMD_UNIT_LEN 32

// event 多媒体控制相关命令的枚举定义
enum {
    C_MMI_CMD_MULTIMEDIA_UNKNOWN = 0,
    C_MMI_CMD_MULTIMEDIA_PLAY,
    C_MMI_CMD_MULTIMEDIA_START_OVER,
    C_MMI_CMD_MULTIMEDIA_STOP,
    C_MMI_CMD_MULTIMEDIA_RESUME_PLAY,
    C_MMI_CMD_MULTIMEDIA_NEXT,
    C_MMI_CMD_MULTIMEDIA_PREVIOUS,
    C_MMI_CMD_MULTIMEDIA_CHANGE
};

// params 多媒体控制相关命令的参数定义
typedef struct {
    char unit[C_MMI_CMD_UNIT_LEN];
} c_mmi_cmd_multimedia_param_t;
/*************************** 多媒体控制组件结束 ***************************/

/*************************** 拍照录像组件开始 ***************************/
// event 拍照录像相关命令的枚举定义
enum {
    C_MMI_CMD_CAMERA_UNKNOWN = 0,
    C_MMI_CMD_CAMERA_TAKE_PHOTO,
    C_MMI_CMD_CAMERA_QUICK_BURST,
    C_MMI_CMD_CAMERA_OPEN_CAMERA,
    C_MMI_CMD_CAMERA_QUIT_CAMERA,
    C_MMI_CMD_CAMERA_OPEN_PHOTO_MODE,
    C_MMI_CMD_CAMERA_QUIT_PHOTO_MODE,
    C_MMI_CMD_CAMERA_OPEN_PREVIEW,
    C_MMI_CMD_CAMERA_QUIT_PREVIEW,
    C_MMI_CMD_CAMERA_VIDEO_RECORDING,
    C_MMI_CMD_CAMERA_OPEN_VIDEO_MODE,
    C_MMI_CMD_CAMERA_QUIT_VIDEO_MODE,
};

// params 拍照录像相关命令的参数定义
typedef struct {
    uint32_t number;
} c_mmi_cmd_quick_burst_param_t;
/*************************** 拍照录像组件结束 ***************************/

/*************************** 电话组件开始 ***************************/
#define C_MMI_CMD_TELEPHONE_LEN 64
// event 电话相关命令的枚举定义
enum {
    C_MMI_CMD_TELEPHONE_UNKNOWN = 0,
    C_MMI_CMD_TELEPHONE_CALL,
    C_MMI_CMD_TELEPHONE_CONFIRM,
    C_MMI_CMD_TELEPHONE_CANCEL,
    C_MMI_CMD_TELEPHONE_OPEN_CALL,
    C_MMI_CMD_TELEPHONE_QUIT_CALL,
    C_MMI_CMD_TELEPHONE_ANSWER_CALL,
    C_MMI_CMD_TELEPHONE_REJECT_PHONE,
    C_MMI_CMD_TELEPHONE_UPDATE_CONTACTS,
};

// params 电话相关命令的参数定义
//  call 指令参数
typedef struct {
    char contact_name[C_MMI_CMD_TELEPHONE_LEN];
    char phone_number[C_MMI_CMD_TELEPHONE_LEN];
    char phone_type[C_MMI_CMD_TELEPHONE_LEN];
    char phone_entity[C_MMI_CMD_TELEPHONE_LEN];
    bool record; // 默认false
} c_mmi_cmd_call_param_t;

// confirm 指令参数
typedef struct {
    bool record; // 默认false
} c_mmi_cmd_confirm_param_t;

// answer_call 指令参数
typedef struct {
    char contact_name[C_MMI_CMD_TELEPHONE_LEN];
    bool record; // 默认false
} c_mmi_cmd_answer_call_param_t;
/*************************** 电话组件结束 ***************************/

/*************************** 录音组件开始 ***************************/
// event 录音相关命令的枚举定义
enum {
    C_MMI_CMD_RECORDING_UNKNOWN = 0,
    C_MMI_CMD_RECORDING_AUDIO_RECORDING,
    C_MMI_CMD_RECORDING_QUIT_AUDIO_RECORDING,
    C_MMI_CMD_RECORDING_STOP_AUDIO_RECORDING,
};

// 音频类型枚举
enum {
    C_MMI_CMD_AUDIO_TYPE_UNKNOWN = 0,
    C_MMI_CMD_AUDIO_TYPE_INTERNAL, // 内部
    C_MMI_CMD_AUDIO_TYPE_EXTERNAL, // 外部
    C_MMI_CMD_AUDIO_TYPE_CALL,     // 通话
};

// params 录音相关命令的参数定义
typedef struct {
    uint32_t audio_type; // 枚举值: C_MMI_CMD_AUDIO_TYPE_XXX
} c_mmi_cmd_audio_recording_param_t;

/*************************** 录音组件结束 ***************************/

/*************************** 应用开关组件开始 ***************************/
#define C_MMI_CMD_APP_LEN 128

// event 应用开关相关命令的枚举定义
enum {
    C_MMI_CMD_APP_UNKNOWN = 0,
    C_MMI_CMD_APP_OPEN_NOTIFICATION,
    C_MMI_CMD_APP_QUIT_NOTIFICATION,
    C_MMI_CMD_APP_CLEAN_NOTIFICATION,
    C_MMI_CMD_APP_OPEN_PHOTOS,
    C_MMI_CMD_APP_QUIT_PHOTOS,
    C_MMI_CMD_APP_OPEN_PLAYER,
    C_MMI_CMD_APP_QUIT_PLAYER,
    C_MMI_CMD_APP_OPEN_APP_CENTER,
    C_MMI_CMD_APP_QUIT_APP_CENTER,
    C_MMI_CMD_APP_OPEN_PROMPTER,
    C_MMI_CMD_APP_QUIT_PROMPTER,
    C_MMI_CMD_APP_OPEN_APP,
    C_MMI_CMD_APP_QUIT_APP,
    C_MMI_CMD_APP_OPEN_SETTING,
    C_MMI_CMD_APP_QUIT_SETTING,
    C_MMI_CMD_APP_OPEN_SYSTEM_UPDATE,
    C_MMI_CMD_APP_QUIT_SYSTEM_UPDATE,
    C_MMI_CMD_APP_OPEN_DND_MODE,
    C_MMI_CMD_APP_QUIT_DND_MODE,
    C_MMI_CMD_APP_OPEN_AUTO_BRIGHTNESS,
    C_MMI_CMD_APP_QUIT_AUTO_BRIGHTNESS,
    C_MMI_CMD_APP_OPEN_VR_CALIBRATION,
    C_MMI_CMD_APP_QUIT_VR_CALIBRATION,
};

// params 应用开关相关命令的参数定义
//  for open_app, quit_app
typedef struct {
    char app_name[C_MMI_CMD_APP_LEN];
} c_mmi_cmd_app_name_param_t;

// for open_setting, quit_setting
typedef struct {
    char type[C_MMI_CMD_APP_LEN];
} c_mmi_cmd_app_setting_param_t;
/*************************** 应用开关组件结束 ***************************/

#ifdef __cplusplus
}
#endif

#endif
