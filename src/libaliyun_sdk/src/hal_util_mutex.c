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

#include "c_utils.h"
#include "tal_mutex.h"
#include "tal_memory.h"

#define MUTEX_WAIT_FOREVER 0x7FFFFFFF

// External log level variable
extern uint8_t g_util_log_lv;

/*****************************************************
 * Function: util_mutex_create
 * Description: 创建一个互斥锁对象。
 * Parameter: 无。
 * Return: util_mutex_t * --- 返回指向互斥锁结构体的指针。
 ****************************************************/
util_mutex_t *util_mutex_create(void)
{
    util_mutex_t *mutex = (util_mutex_t *)util_malloc(sizeof(util_mutex_t));
    if (mutex == NULL) {
        UTIL_LOG_E("Failed to allocate memory for mutex");
        return NULL;
    }

    MUTEX_HANDLE tal_mutex_handle;
    OPERATE_RET ret = tal_mutex_create_init(&tal_mutex_handle);
    if (ret != OPRT_OK) {
        UTIL_LOG_E("Failed to create mutex: %d", ret);
        util_free(mutex);
        return NULL;
    }

    mutex->mutex_handle = (void *)tal_mutex_handle;
    UTIL_LOG_D("Mutex created successfully");
    return mutex;
}

/*****************************************************
 * Function: util_mutex_delete
 * Description: 删除指定的互斥锁对象。
 * Parameter:
 *     mutex --- 指向互斥锁结构体的指针。
 * Return: 无。
 ****************************************************/
void util_mutex_delete(util_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }

    if (mutex->mutex_handle != NULL) {
        tal_mutex_release((MUTEX_HANDLE)mutex->mutex_handle);
    }

    util_free(mutex);
    UTIL_LOG_D("Mutex deleted successfully");
}

/*****************************************************
 * Function: util_mutex_lock
 * Description: 对指定的互斥锁进行加锁操作，带超时机制。
 * Parameter:
 *     mutex --- 指向互斥锁结构体的指针。
 *     timeout --- 加锁等待超时时间，单位为毫秒(ms)，可设为 MUTEX_WAIT_FOREVER 表示无限等待。
 * Return: int32_t --- 返回操作结果(util_result_t)。
 ****************************************************/
int32_t util_mutex_lock(util_mutex_t *mutex, int32_t timeout)
{
    UTIL_NULL_CHECK(mutex, UTIL_ERR_INVALID_PARAM);

    if (mutex->mutex_handle == NULL) {
        UTIL_LOG_E("Invalid mutex handle");
        return UTIL_ERR_INVALID_PARAM;
    }

    OPERATE_RET ret = tal_mutex_lock((MUTEX_HANDLE)mutex->mutex_handle);
    if (ret != OPRT_OK) {
        UTIL_LOG_E("Failed to lock mutex: %d", ret);
        return UTIL_ERR_FAIL;
    }

    return UTIL_SUCCESS;
}

/*****************************************************
 * Function: util_mutex_unlock
 * Description: 对指定的互斥锁进行解锁操作。
 * Parameter:
 *     mutex --- 指向互斥锁结构体的指针。
 * Return: int32_t --- 返回操作结果(util_result_t)。
 ****************************************************/
int32_t util_mutex_unlock(util_mutex_t *mutex)
{
    UTIL_NULL_CHECK(mutex, UTIL_ERR_INVALID_PARAM);

    if (mutex->mutex_handle == NULL) {
        UTIL_LOG_E("Invalid mutex handle");
        return UTIL_ERR_INVALID_PARAM;
    }

    OPERATE_RET ret = tal_mutex_unlock((MUTEX_HANDLE)mutex->mutex_handle);
    if (ret != OPRT_OK) {
        UTIL_LOG_E("Failed to unlock mutex: %d", ret);
        return UTIL_ERR_FAIL;
    }

    return UTIL_SUCCESS;
}
