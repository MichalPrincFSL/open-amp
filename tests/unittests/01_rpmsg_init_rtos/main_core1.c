#include "unity.h"
#include "openamp/open_amp.h"
#include "openamp/rpmsg_rtos.h"
#include "assert.h"

struct rpmsg_channel *app_chnl = NULL;
struct remote_device *rdev = NULL;
int test_counter = 0;

void tc_1_rpmsg_init(void)
{
    int result;

    for (test_counter = 0; test_counter < 50; test_counter++)
    {
        env_init();
        result = rpmsg_rtos_init(0, &rdev, RPMSG_MASTER, &app_chnl);
        TEST_ASSERT_MESSAGE(0 == result, "init function failed");

        TEST_ASSERT_MESSAGE(NULL != app_chnl, "check channel");
        TEST_ASSERT_MESSAGE(NULL != rdev, "check device");

        rpmsg_rtos_deinit(rdev);
        env_deinit();
        app_chnl = NULL;
        rdev = NULL;
    }
}

void run_tests()
{
    RUN_TEST(tc_1_rpmsg_init, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
