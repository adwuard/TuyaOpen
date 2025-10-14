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

#ifndef __LIB_C_MMI_H__
#define __LIB_C_MMI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "c_utils/c_utils.h"

enum {
    C_MMI_EVENT_USER_CONFIG, // 用户对于sdk的配置应该在该事件回调中实现，如音频缓冲区大小、工作模式、音色等
    C_MMI_EVENT_DATA_INIT, // 当SDK完成初始化后触发该事件，可在该事件回调中开始建立业务连接
    C_MMI_EVENT_SPEECH_START, // 当SDK开始进行音频上行时触发此事件
    C_MMI_EVENT_DATA_DEINIT,  // 当SDK注销后触发此事件

    C_MMI_EVENT_ASR_START,      // 当ASR开始返回数据时触发此事件
    C_MMI_EVENT_ASR_INCOMPLETE, // 此事件返回尚未完成ASR的文本数据（全量）
    C_MMI_EVENT_ASR_COMPLETE,   // 此事件返回完成ASR的全部文本数据（全量）
    C_MMI_EVENT_ASR_END,        // 当ASR结束时触发此事件

    C_MMI_EVENT_LLM_INCOMPLETE, // 此事件返回尚未处理完成的LLM文本数据（全量）
    C_MMI_EVENT_LLM_COMPLETE,   // 此事件返回处理完成的LLM全部文本数据（全量）

    C_MMI_EVENT_TTS_START, // 当开始音频下行时触发此事件
    C_MMI_EVENT_TTS_END,   // 当音频完成下行时触发此事件
};

typedef struct _c_mmi_prompt_pram_t {
    char *key;
    char *value;
} c_mmi_prompt_pram_t;

/**
 * @brief 获取WSS服务器主机名字符串。
 *
 * 此函数返回配置的WSS服务器主机名字符串。
 *
 * @return char*
 *         返回指向主机名字符串的指针，若未配置则返回NULL。
 */
char *c_mmi_get_wss_host(void);

/**
 * @brief 获取WSS服务器端口字符串。
 *
 * 此函数返回配置的WSS服务器端口字符串。
 *
 * @return char*
 *         返回指向端口字符串的指针，若未配置则返回NULL。
 */
char *c_mmi_get_wss_port(void);

/**
 * @brief 获取WSS服务API路径字符串。
 *
 * 此函数返回配置的WSS服务API路径字符串。
 *
 * @return char*
 *         返回指向API路径字符串的指针，若未配置则返回NULL。
 */
char *c_mmi_get_wss_api(void);

/**
 * @brief 获取WSS请求头信息字符串。
 *
 * 此函数返回配置的WSS请求头信息字符串。
 *
 * @return char*
 *         返回指向请求头字符串的指针，若未配置则返回NULL。
 */
char *c_mmi_get_wss_header(void);

/**
 * @brief 设置设备的IP地址信息
 *
 * 此函数用于设置设备的IP地址，该信息将被包含在客户端信息中发送给服务端
 *
 * @param ip 指向IP地址字符串的指针
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_set_ip(char *ip);

/**
 * @brief 设置设备的地理位置坐标
 *
 * 此函数用于设置设备的经度和纬度信息，该信息将被包含在客户端信息中发送给服务端
 *
 * @param longitude 经度值
 * @param latitude 纬度值
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_set_postion(double longitude, double latitude);

/**
 * @brief 设置设备所在城市信息
 *
 * 此函数用于设置设备所在的城市信息，该信息将被包含在客户端信息中发送给服务端
 *
 * @param city 指向城市名称字符串的指针
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_set_city(char *city);

/**
 * @brief 反初始化MMI模块
 *
 * 此函数用于释放MMI模块占用的所有资源，并重置模块状态
 *
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_deinit(void);

/**
 * @brief 配置提示词参数
 *
 * 此函数用于配置用户自定义的提示词参数
 *
 * @param param_list 指向提示词参数结构体数组的指针
 * @param list_size 参数数组中元素的个数
 * @return int32_t 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_data_set_prompt_param(c_mmi_prompt_pram_t *param_list, uint32_t list_size);

/**
 * @brief 向录音缓冲区写入数据
 *
 * 当音频模式为C_mmi_AUDIO_MODE_RINGBUFFER时使用
 *
 * @param data 待写入的数据指针
 * @param size 数据长度（字节）
 * @return uint32_t 实际写入的字节数
 */
uint32_t c_mmi_put_recorder_data(uint8_t *data, uint32_t size);

/**
 * @brief 从播放缓冲区读取数据
 *
 * 当音频模式为C_mmi_AUDIO_MODE_RINGBUFFER时使用
 *
 * @param data 用于存储读取数据的缓冲区
 * @param size 可用缓冲区大小（字节）
 * @return uint32_t 实际读取的字节数
 */
uint32_t c_mmi_get_player_data(uint8_t *data, uint32_t size);

/********************** 以下为旧版接口，下列接口将在2025-12-31弃用 **********************/
int32_t c_mmi_data_deinit(void); // 使用 c_mmi_deinit 替代
/********************** 旧版接口结束 **********************/

#include "lib_c_mmi_config.h"
#include "lib_c_mmi_msg.h"

#ifdef __cplusplus
}
#endif

#endif
