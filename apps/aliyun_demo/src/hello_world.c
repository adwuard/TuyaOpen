#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "lib_c_sdk.h"
#include "aliyun_test_api.h"

// Official Aliyun SDK HAL layer validation test
static void run_aliyun_sdk_hal_test(void)
{
    PR_INFO("=== Starting Aliyun SDK HAL Layer Validation Test ===");
    PR_INFO("This test validates all HAL implementations according to Aliyun SDK requirements");
    PR_INFO("Test includes: Memory, Time, Storage, Random, Mutex management");

    // Call the official Aliyun SDK test function
    int32_t test_result = aliyun_sdk_test_api();

    if (test_result == 0) {
        PR_INFO("=== Aliyun SDK HAL Test: PASSED ===");
        PR_INFO("All HAL implementations are working correctly!");
        PR_INFO("The output logs above show the detailed test results.");
    } else {
        PR_ERR("=== Aliyun SDK HAL Test: FAILED ===");
        PR_ERR("Test failed with error code: %d", test_result);
        PR_ERR("Please check the logs above for specific failure details.");
    }

    PR_INFO("=== Aliyun SDK HAL Validation Test Complete ===");
}

static void user_main(void)
{
    // Initialize TuyaOpen TAL logging system
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_INFO("TuyaOpen TAL Logging System Initialized");

    // Initialize Aliyun SDK logging to use TuyaOpen's system
    // Note: Using TAL logging system instead of util_set_log_level
    PR_INFO("Aliyun SDK Logging Integrated with TuyaOpen");

    PR_INFO("=== Aliyun SDK + TuyaOpen Demo Started ===");

    // Run the official Aliyun SDK HAL validation test
    run_aliyun_sdk_hal_test();

    int cnt = 0;
    while (1) {
        PR_DEBUG("Demo running, count: %d", cnt++);
        tal_system_sleep(5000);
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