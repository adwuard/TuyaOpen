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

#ifndef __LIB_C_MMI_CONFIG_H__
#define __LIB_C_MMI_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define C_MMI_VOICE_ID_LEN (64) // max 64

enum { C_MMI_MODE_NONE, C_MMI_MODE_PUSH2TALK, C_MMI_MODE_TAP2TALK, C_MMI_MODE_DUPLEX, C_MMI_MODE_MAX };

enum { C_MMI_TEXT_MODE_NONE, C_MMI_TEXT_MODE_ASR_ONLY, C_MMI_TEXT_MODE_LLM_ONLY, C_MMI_TEXT_MODE_BOTH };

enum {
    C_MMI_STREAM_MODE_PCM,
    C_MMI_STREAM_MODE_OPUS_OGG,
    C_MMI_STREAM_MODE_MP3,
    C_MMI_STREAM_MODE_MAX,
};

enum {
    C_MMI_OPUS_MODE_OGG,
    C_MMI_OPUS_MODE_RAW,
    C_MMI_OPUS_MODE_MAX,
};

/**
 * @brief mmi事件回调函数类型
 *
 * 当mmi模块发生状态变化或事件时触发的回调函数
 *
 * @param event 事件类型，取值为C_MMI_EVENT_xxx系列宏定义
 * @param param 事件参数，根据事件类型不同指向不同数据结构
 * @return int32_t 返回0表示处理成功，非0表示处理失败
 */
typedef int32_t (*c_mmi_event_callback)(uint32_t event, void *param);

typedef struct _mmi_user_config_t {
    // 用户设置
    c_mmi_event_callback evt_cb;

    uint8_t work_mode;
    uint8_t text_mode;
    char voice_id[C_MMI_VOICE_ID_LEN];

    uint8_t upstream_mode;   // 支持 C_MMI_STREAM_MODE_PCM/C_MMI_STREAM_MODE_OPUS_OGG
    uint8_t downstream_mode; // 支持 C_MMI_STREAM_MODE_PCM/C_MMI_STREAM_MODE_OPUS_OGG/C_MMI_STREAM_MODE_MP3

    uint8_t upstream_out_format;   // 该参数仅在opus模式下有效
    uint8_t downstream_out_format; // 该参数仅在opus模式下有效

    uint32_t recorder_rb_size;
    uint32_t player_rb_size;

} mmi_user_config_t;

/**
 * @brief 配置MMI模块参数
 *
 * 此函数用于初始化MMI模块的各项配置参数，包括事件回调、工作模式、文本模式、
 * 音色设置、音频流模式以及缓冲区大小等
 *
 * @param config 指向mmi_user_config_t结构体的指针，包含配置参数
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_config(mmi_user_config_t *config);

/**
 * @brief 设置voice_id，可设置克隆音色
 *
 * 此函数用于设置模型回复语音音色
 *
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_set_voice_id(char *voice_id);

/**
 * @brief 禁用licesnse模式，使用后付费模式
 *
 * 此函数用于将付费模式从预付费切换为后付费模式
 *
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_license_disable(void);

/********************** 以下为旧版接口，下列接口将在2025-12-31弃用 **********************/
// 以下为旧版接口，不推荐使用，推荐使用c_mmi_config
int32_t c_mmi_register_event_callback(c_mmi_event_callback cb); // 使用 c_mmi_config 替代
int32_t c_mmi_set_work_mode(uint8_t work_mode);                 // 使用 c_mmi_config 替代
int32_t c_mmi_set_text_mode(uint8_t text_mode);                 // 使用 c_mmi_config 替代
int32_t c_mmi_set_upstream_mode(uint8_t stream_mode);           // 使用 c_mmi_config 替代
int32_t c_mmi_set_downstream_mode(uint8_t stream_mode);         // 使用 c_mmi_config 替代
int32_t c_mmi_set_audio_mode_ringbuffer(uint32_t recorder_rb_size, uint32_t player_rb_size); // 使用 c_mmi_config 替代
/********************** 旧版接口结束 **********************/

#ifdef __cplusplus
}
#endif

#endif
