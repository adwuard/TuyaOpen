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

#ifndef __LIB_C_MMI_MSG_H__
#define __LIB_C_MMI_MSG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 获取需要通过websocket发送的数据
 *
 * 本函数用于根据指定的类型获取数据，准备通过websocket进行发送
 * 它会根据传入的类型参数，将相应类型的数据填充到提供的数据缓冲区中
 *
 * @param opcode 用于返回websocket数据类型，如：WEBSOCKET_OPCODE_TEXT、WEBSOCKET_OPCODE_BINARY
 * @param data 指向一个uint8_t数组的指针，该数组用于存储获取的数据
 * @param size 表示数据数组的最大容量，以字节为单位
 *
 * @return 返回实际填充到数据数组中的字节数
 */
uint32_t c_mmi_get_send_data(uint8_t *opcode, uint8_t *data, uint32_t size);

/**
 * 分析接收到的websocket数据函数
 *
 * 此函数根据提供的数据类型和数据内容，分析接收到的数据包
 * 它的主要作用是解析数据内容，以便进一步处理或使用
 *
 * @param opcode websocket数据类型，如：WEBSOCKET_OPCODE_TEXT、WEBSOCKET_OPCODE_BINARY
 * @param data 指向接收到的数据的指针，数据的内容将根据type参数进行解析
 * @param size 数据的长度，以字节为单位，用于确定数据的范围
 *
 * @return 返回实际解析的字节数
 */
uint32_t c_mmi_analyze_recv_data(uint8_t opcode, uint8_t *data, uint32_t size);

/**
 * @brief 返回是否已接收所有音频数据
 *
 * 该函数播放器判断是否还有音频数据需要播放
 *
 * @param void 无参数
 *
 * @return uint8_t 返回接收状态或结果
 *         - 0: 还有音频数据待读取
 *         - 1: 数据已全被接收
 */
uint8_t c_mmi_audio_recv_all(void);

/**
 * @brief 发送文本query到MMI模块进行处理
 *
 * 此函数用于大模型将会根据传入的 text 文本进行回答，并将回答转换成TTS语音下发
 * 通常用于实现文本交互功能
 *
 * @param text 指向包含问题文本的字符串指针
 * @return 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_question(char *text);

/**
 * @brief 将文本转换为语音播放
 *
 * 此函数用于大模型将传入的 text 文本字段转换成TTS语音下发
 * 实现文本到语音(TTS)的功能
 *
 * @param text 指向包含待转换文本的字符串指针
 * @return 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_tts(char *text);

/**
 * @brief 暂停心跳功能
 *
 * 该函数用于暂停SDK的心跳机制
 *
 * @param void 无参数
 *
 * @return 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_heartbeat_suspend(void);

/**
 * @brief 恢复心跳功能
 *
 * 该函数用于恢复SDK的心跳机制
 *
 * @param void 无参数
 *
 * @return 返回操作结果，0表示成功，非0表示失败
 */
int32_t c_mmi_heartbeat_resume(void);

/**
 * 开始语音上行。
 *
 * 本函数用于通知云端，端侧开始进行语音数据上行。
 * 该接口可退出等待状态，也可打断当前正在进行的对话。
 * 当还处于拾音状态时，此接口无效。
 *
 * @return 成功语音识别时返回0，否则返回非零的错误码。
 */
int32_t c_mmi_speech_start(void);

/**
 * 结束语音上行。
 *
 * 本函数用于通知云端，端侧语音上行已结束。
 * 该进口仅在push2talk模式下有效。
 *
 * @return 成功语音识别时返回0，否则返回非零的错误码。
 */
int32_t c_mmi_speech_end(void);

/**
 * 停止语音识别功能。
 *
 * 本函数用于中断语音识别模块，进入等待状态。
 *
 * @return 成功语音识别时返回0，否则返回非零的错误码。
 */
int32_t c_mmi_speech_pause(void);

/********************** 以下为旧版接口，下列接口将在2025-12-31弃用 **********************/
int32_t c_mmi_start_speech(void); // 使用 c_mmi_speech_start 替代
int32_t c_mmi_stop_speech(void);  // 使用 c_mmi_speech_end 替代
int32_t c_mmi_pause_task(void);   // 使用 c_mmi_speech_pause 替代
/********************** 旧版接口结束 **********************/

#ifdef __cplusplus
}
#endif

#endif
