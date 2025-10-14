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

#ifndef __LIB_C_SDK_H__
#define __LIB_C_SDK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "c_utils/c_utils.h"
#include "lib_c_storage.h"
#include "lib_c_mmi_cmd.h"

#define C_SDK_REQ_LEN_REGISTER  500
#define C_SDK_REQ_LEN_GET_TOKEN 500

enum {
    WEBSOCKET_OPCODE_TEXT = 0x01,
    WEBSOCKET_OPCODE_BINARY = 0x02,
    WEBSOCKET_OPCODE_DISCONNECT = 0x08,
    WEBSOCKET_OPCODE_PING = 0x09,
    WEBSOCKET_OPCODE_PONG = 0x0A,
};

/**
 * 初始化阿里云SDK。
 *
 * 返回: 初始化结果，0表示成功，非0表示失败。
 */
int32_t c_mmi_sdk_init(void);

/**
 * 生成设备注册请求所需的字符串。
 *
 * 参数 req_str: 用于存储生成的设备注册请求字符串的缓冲区。
 * 参数 time_ms_str: 时间戳字符串，单位为毫秒
 *
 * 返回: 生成结果，0表示成功，非0表示失败。
 */
int32_t c_device_gen_register_req(char req_str[C_SDK_REQ_LEN_REGISTER], char *time_ms_str);

/**
 * 解析云端的设备注册响应信息。
 *
 * 参数 rsp_str: 指向设备注册响应字符串的指针。
 *
 * 返回: 分析结果，0表示成功，非0表示失败。
 */
int32_t c_device_analyze_register_rsp(char *rsp_str);

/**
 * @brief 检查mmi登录状态。
 *
 * 返回值表示当前mmi是否已完成登录流程，1为已登录，0为未登录。
 *
 * @return uint8_t
 *         返回1表示已登录，返回0表示未登录。
 */
uint8_t c_mmi_is_token_expire(void);

/**
 * @brief 生成mmi登录请求数据。
 *
 * 此函数根据提供的当前时间戳生成mmi登录请求字符串，
 *
 *
 * @param req_str 存储生成的登录请求字符串的缓冲区
 * @param time_ms_str 时间戳字符串，单位为毫秒
 * @return int32_t
 *         成功返回0，失败返回非零的错误码
 */
int32_t c_mmi_gen_get_token_req(char req_str[C_SDK_REQ_LEN_GET_TOKEN], char *time_ms_str);

/**
 * @brief 解析mmi登录响应数据。
 *
 * 解析登录响应字符串，并触发事件：
 * - C_MMI_EVENT_USER_CONFIG: 用户配置初始化
 * - C_MMI_EVENT_DATA_INIT: 数据初始化完成
 *
 * @param rsp_str 包含mmi登录响应的字符串
 * @return int32_t
 *         成功返回0，失败返回非零的错误码
 */
int32_t c_mmi_analyze_get_token_rsp(char *rsp_str);

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

#include "lib_c_mmi.h"

/********************** 以下为旧版接口，下列接口将在2025-12-31弃用 **********************/
enum {
    WS_DATA_TYPE_TEXT = 0x01,       // 使用 WEBSOCKET_OPCODE_TEXT 替代
    WS_DATA_TYPE_BINARY = 0x02,     // 使用 WEBSOCKET_OPCODE_BINARY 替代
    WS_DATA_TYPE_DISCONNECT = 0x08, // 使用 WEBSOCKET_OPCODE_DISCONNECT 替代
    WS_DATA_TYPE_PING = 0x09,       // 使用 WEBSOCKET_OPCODE_PING 替代
    WS_DATA_TYPE_PONG = 0x0A,       // 使用 WEBSOCKET_OPCODE_PONG 替代
};
/********************** 旧版接口结束 **********************/

#ifdef __cplusplus
}
#endif

#endif
