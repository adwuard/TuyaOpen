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
#include "tal_system.h"

// Static variable to track initialization state
static uint8_t s_random_initialized = 0;

/**
 * 初始化随机数生成器
 *
 * @param seed 用于初始化随机数生成器的种子值
 *
 * @return 返回初始化结果，0表示成功，非0表示失败
 *
 * 此函数通过对随机数生成器进行初始化，以确保后续生成的随机数序列具有良好的随机性
 * 种子值的选择对生成的随机数序列有重要影响，相同的种子值会生成相同的随机数序列
 */
int32_t util_random_init(uint32_t seed)
{
    // TuyaOpen TAL system random function doesn't require explicit initialization
    // The seed parameter is ignored as the system provides hardware-based randomness
    s_random_initialized = 1;

    UTIL_LOG_D("Random number generator initialized (seed: %u)", seed);
    return UTIL_SUCCESS;
}

/**
 * 生成一个随机数
 *
 * @return 返回生成的随机数
 *
 * 在调用此函数之前，应确保随机数生成器已经通过util_random_init函数成功初始化
 * 此函数生成的随机数是基于初始化时提供的种子值产生的
 */
uint32_t util_random(void)
{
    if (!s_random_initialized) {
        // Auto-initialize if not already done
        util_random_init(0);
    }

    // Use TuyaOpen TAL system random function
    // Generate a random number in the range [0, UINT32_MAX]
    uint32_t random_value = tal_system_get_random(UINT32_MAX);

    return random_value;
}
