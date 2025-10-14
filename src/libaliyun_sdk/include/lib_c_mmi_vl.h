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
/**
 *FileName:   lib_c_mmi_vl.h
 *Date:       2025-07-16 19:16:44
 *Version:    1.0
 *Path:       ali_sdk/include/lib_c_mmi_vl.h
 */
#ifndef __LIB_C_MMI_VL_H_
#define __LIB_C_MMI_VL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "c_utils/c_utils.h"

//  VL模块支持分辨率
enum {
    C_MMI_VL_FRAMESIZE_96x96,
    C_MMI_VL_FRAMESIZE_160x120,
    C_MMI_VL_FRAMESIZE_128x128,
    C_MMI_VL_FRAMESIZE_176x144,
    C_MMI_VL_FRAMESIZE_240x176,
    C_MMI_VL_FRAMESIZE_240x240,
    C_MMI_VL_FRAMESIZE_320x240,
    C_MMI_VL_FRAMESIZE_320x320,
    C_MMI_VL_FRAMESIZE_400x296,
    C_MMI_VL_FRAMESIZE_480x320,
    C_MMI_VL_FRAMESIZE_640x480,
    C_MMI_VL_FRAMESIZE_800x600,
    C_MMI_VL_FRAMESIZE_1024x768,
    C_MMI_VL_FRAMESIZE_1280x720,
    C_MMI_VL_FRAMESIZE_1280x1024,
    C_MMI_VL_FRAMESIZE_1600x1200,
    C_MMI_VL_FRAMESIZE_1920x1080,
    C_MMI_VL_FRAMESIZE_720x1280,
    C_MMI_VL_FRAMESIZE_864x1536,
    C_MMI_VL_FRAMESIZE_2560x1440,
    C_MMI_VL_FRAMESIZE_2560x1600,
    C_MMI_VL_FRAMESIZE_1080x1920,
    C_MMI_VL_FRAMESIZE_2560x1920,
    C_MMI_VL_FRAMESIZE_2592x1944,
    C_MMI_VL_FRAMESIZE_INVALID,
};

//  VL模块支持图片格式
enum {
    C_MMI_VL_PIC_FORMAT_JPEG,
    C_MMI_VL_PIC_FORMAT_BMP,
    C_MMI_VL_PIC_FORMAT_PNG,
    C_MMI_VL_PIC_FORMAT_TIF,
    C_MMI_VL_PIC_FORMAT_WEBP,
    C_MMI_VL_PIC_FORMAT_HEIC,
    C_MMI_VL_PIC_FORMAT_JPG = C_MMI_VL_PIC_FORMAT_JPEG,
    C_MMI_VL_PIC_FORMAT_JPE = C_MMI_VL_PIC_FORMAT_JPEG,
    C_MMI_VL_PIC_FORMAT_TIFF = C_MMI_VL_PIC_FORMAT_TIF
};

//  VL模块工作模式
enum {
    C_MMI_VL_MODE_NONE = 1 << 0,
    C_MMI_VL_MODE_VQA = 1 << 1,
    C_MMI_VL_MODE_LIVE_AI = 1 << 2,
};

//  VL模块传入数据类型
enum {
    C_MMI_VL_DATA_BASE64,
    C_MMI_VL_DATA_URL,
};

//  VL模块回调事件
enum {
    C_MMI_VL_EVENT_VQA_START, // 此事件在SDK开启VQA触发，建议在该事件回调中开始拍照，并将将拍摄的照片送入VL数据缓存区
    C_MMI_VL_EVENT_VQA_END,   // 此事件在SDK结束VQA触发，建议在该事件回调关闭摄像头
    C_MMI_VL_EVENT_LIVEAI_START, // 此事件在SDK中开启LIVEAI时触发，建议在该事件回调触开启摄像头
    C_MMI_VL_EVENT_LIVEAI_ACTION, // 此事件在SDK中LIVEAI触发抽帧时触发，建议在该事件回调触发后的超时时间内保证VL数据缓冲区有图像数据
    C_MMI_VL_EVENT_LIVEAI_STOP,   // 此事件在SDK中关闭LIVEAI时触发，建议在该事件回调关闭摄像头
    C_MMI_VL_EVENT_TIMEOUT, // 此事件在SDK的VL等待数据超时时触发，建议在该事件回调中关闭摄像头并释放其所有资源
};

/**
 * @brief 设置VL标准模式下的参数
 *
 * @param frame_size 	    传入图片的分辨率大小
 * @param pic_format    传入图片格式
 * @param vl_mode
 *		  VL工作的模式，分为待机模式、标准模式和LIVE AI视频通话模式，目前视频通话不可用；
 *        待机模式下整个模块都会停止工作；
 *		  标准模式下进行常规VL问答；
 * 		  视频模式暂不可用；
 * @param data_type
 *		  传入图片数据的形式，分为base64和url形式，需要在初始化时指定。
 * @param buffer_size
 *        设置VL数据缓存区大小，单位字节，默认1024字节；如果要使用base64数据，请设置较大缓存区。
 *        云端单张照片目前要求小于180KiB，请查阅百炼相关最新文档说明。
 * @param fps
 *		  当相机开始录像工作后，VL模块将以每1/fps秒一张图片的速度从外界获取图片并上传至云端,此选项只在LIVE
 *AI模式下有效。 取值为0-无穷，默认为2，取值0等价为默认值;鉴于云端文档说明，目前建议FPS固定为2。
 * @param timeout_ms
 *        设置VL模块在等待数据采集时的最长闲置时间，当闲置时间超过设置值时将会触发C_MMI_VL_EVENT_TIMEOUT事件并结束VL流程，
 *        闲置时间单位为ms，取值为0-无穷，默认值为0，0表示无限等待。
 *  @param event_callback
 *        注册VL事件回调函数
 */
typedef struct {
    uint8_t frame_size;
    uint8_t pic_format;
    uint8_t vl_mode;
    uint8_t data_type;
    uint32_t buffer_size;
    uint32_t timeout_ms;
    uint32_t fps;
    int32_t (*event_callback)(uint32_t event, void *param);
} vl_config_t;

/**
 * @brief 向VL缓冲区写入URL数据，
 *		  URL需要加入鉴权相关信息保证阿里云可以通过链接访问到照片
 *
 * @param url  待写入的数据指针
 * @param url_size 每次调用时写入的URL字符串数据长度（字节）
 * @return uint32_t 实际写入的字节数
 */
uint32_t c_mmi_put_vl_data_url(uint8_t *url, uint32_t url_size);

/**
 * @brief 向VL缓冲区写入base64编码的图片数据，支持分片输入
 *
 * @param data 待写入的数据指针
 * @param size 每次调用时写入的base64编码图片数据长度（字节）
 * @param finish 当前照片数据是否传输完成，0代表当前数据没有传入完成，
 * 				 1代表当前1帧完整图片已经传输完成
 * @return uint32_t 实际写入的字节数
 */
uint32_t c_mmi_put_vl_data_base64(uint8_t *data, uint32_t size, uint8_t finish);

/**
 * @brief 设置VL普通模式下的参数
 *
 * @param vl_normal_config_t
 *        视觉交互模式设置参数，此参数以最后一次设置为准，并且此参数只有在新一轮对话开始时生效
 * @return 0代表成功，非0代表失败
 */
int32_t c_mmi_set_vl_params(vl_config_t *vl_config);

/**
 * @brief 开始LIVE AI流程
 *
 * 当LIVE AI在运行时可调用，此接口可以通知SDK开启视频对话，调用此接口后，SDK开始视频通话流程。
 *
 * @return int32_t 成功返回0，非0表示失败。
 */
int32_t c_mmi_vl_liveai_start(void);

/**
 * @brief 停止LIVE AI流程
 *
 * 当LIVE AI在运行时可调用，此接口可以通知SDK结束视频对话，结束视频通话流程。
 *
 * @return int32_t 成功返回0，非0表示失败。
 */
int32_t c_mmi_vl_liveai_stop(void);

#ifdef __cplusplus
}
#endif
#endif