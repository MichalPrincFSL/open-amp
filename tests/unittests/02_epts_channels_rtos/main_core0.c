#include "unity.h"
#include "openamp/open_amp.h"
#include "openamp/rpmsg_rtos.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"

#define TC_INIT_COUNT 10
#define TC_TRANSFER_COUNT 10
#define TC_EPT_COUNT 3
#define TC_CHANNEL_COUNT 2

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;

// utility: initialize rpmsg and enviroment
// and wait for default channel
int ts_init_rpmsg(void)
{
    env_init();
    env_sleep_msec(200);
    int result = rpmsg_rtos_init(1, &rdev, RPMSG_REMOTE, &app_chnl);
    TEST_ASSERT_MESSAGE(0 == result, "init function failed");
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "init function failed");
    TEST_ASSERT_MESSAGE(NULL != rdev, "init function failed");
    return 0;
}

// utility: deinitialize rpmsg and enviroment
int ts_deinit_rpmsg(void)
{
    rpmsg_rtos_deinit(rdev);
    env_deinit();
    app_chnl = NULL;
    return 0;
}

// utility: create number of epts
int ts_create_epts(struct rpmsg_channel *channel, struct rpmsg_endpoint *epts[], int count, int init_addr)
{
    TEST_ASSERT_MESSAGE(channel != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int i = 0; i < count; i++)
    {
        epts[i] = rpmsg_rtos_create_ept(channel, init_addr + i);
        TEST_ASSERT_MESSAGE(NULL != epts[i], "'rpmsg_create_ept' failed");
        TEST_ASSERT_MESSAGE(init_addr + i == epts[i]->addr, "'rpmsg_create_ept' does not provide expected address");
    }
    return 0;
}

// utility: destroy number of epts
int ts_destroy_epts(struct rpmsg_endpoint *epts[], int count)
{
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int i = 0; i < count; i++)
    {
        rpmsg_rtos_destroy_ept(epts[i]);
    }
    return 0;
}

// utility: create number of epts
int ts_create_channels(struct remote_device *rdev, struct rpmsg_channel *channels[], char *channel_name, int count)
{
    TEST_ASSERT_MESSAGE(channels != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int i = 0; i < count; i++)
    {
        channels[i] = rpmsg_create_channel(rdev, channel_name);
        TEST_ASSERT_MESSAGE(NULL != channels[i], "'rpmsg_create_channel' failed");
    }
    return 0;
}

// utility: destroy number of epts
int ts_destroy_channels(struct rpmsg_channel *channels[], int count)
{
    TEST_ASSERT_MESSAGE(channels != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int i = 0; i < count; i++)
    {
        rpmsg_delete_channel(channels[i]);
    }
    return 0;
}

/******************************************************************************
 * Test case 1
 * - verify simple transport between default epts of default channels
 * - verify simple nocopy transport between default epts of default channels
 * - verify simple transport between custom created epts of default channels
 * - verify simple nocopy transport between custom created epts of default channels
 *****************************************************************************/
void tc_1_defchnl_1(void)
{
    int result = 0;

    // init rpmsg and enviroment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int received = 0, len = 0, send = i;
        unsigned long int src = 0;
        rpmsg_rtos_send(app_chnl->rp_ept, &send, sizeof(send), app_chnl->dst);
        rpmsg_rtos_recv(app_chnl->rp_ept, &received, &len, sizeof(received), &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE(received == i + 10, "'rpmsg_rtos_recv' failed");
        TEST_ASSERT_MESSAGE(src == app_chnl->dst, "'rpmsg_rtos_recv' failed");
        TEST_ASSERT_MESSAGE(len == sizeof(send), "'rpmsg_rtos_recv' failed");
    }

end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl_2(void)
{
    int result = 0;

    // init rpmsg and enviroment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int *received = NULL, len = 0, send = i;
        unsigned long int src = 0;
        rpmsg_rtos_send(app_chnl->rp_ept, &send, sizeof(send), app_chnl->dst);
        rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, (void **)&received, &len, &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE(*received == i + 10, "'rpmsg_rtos_recv' failed");
        TEST_ASSERT_MESSAGE(src == app_chnl->dst, "'rpmsg_rtos_recv' failed");
        TEST_ASSERT_MESSAGE(len == sizeof(send), "'rpmsg_rtos_recv' failed");
        rpmsg_rtos_release_rx_buffer(app_chnl->rp_ept, received);
    }

end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl_3(void)
{
    struct rpmsg_endpoint *epts[TC_EPT_COUNT] = {0};
    int result = 0;

    // init rpmsg and enviroment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    result = ts_create_epts(app_chnl, epts, TC_EPT_COUNT, 10);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        for (int j = 0; j < TC_EPT_COUNT; j++)
        {
            int received = 0, len = 0, send = epts[j]->addr;
            unsigned long int src = 0;
            rpmsg_rtos_send(epts[j], &send, sizeof(send), epts[j]->addr);
            rpmsg_rtos_recv(epts[j], &received, &len, sizeof(received), &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE(received == send + 10, "'rpmsg_rtos_recv' failed");
            TEST_ASSERT_MESSAGE(src == epts[j]->addr, "'rpmsg_rtos_recv' failed");
            TEST_ASSERT_MESSAGE(len == sizeof(send), "'rpmsg_rtos_recv' failed");
        }
    }

end1:
    result = ts_destroy_epts(epts, TC_EPT_COUNT);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_destroy_epts' failed");
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl_4(void)
{
    struct rpmsg_endpoint *epts[TC_EPT_COUNT] = {0};
    int result = 0;

    // init rpmsg and enviroment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    result = ts_create_epts(app_chnl, epts, TC_EPT_COUNT, 10);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        for (int j = 0; j < TC_EPT_COUNT; j++)
        {
            int *received = NULL, len = 0, send = epts[j]->addr;
            unsigned long int src = 0;
            rpmsg_rtos_send(epts[j], &send, sizeof(send), epts[j]->addr);
            rpmsg_rtos_recv_nocopy(epts[j], (void **)&received, &len, &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE(*received == send + 10, "'rpmsg_rtos_recv' failed");
            TEST_ASSERT_MESSAGE(src == epts[j]->addr, "'rpmsg_rtos_recv' failed");
            TEST_ASSERT_MESSAGE(len == sizeof(send), "'rpmsg_rtos_recv' failed");
            rpmsg_rtos_release_rx_buffer(epts[j], received);
        }
    }

end1:
    result = ts_destroy_epts(epts, TC_EPT_COUNT);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_destroy_epts' failed");
end0:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_deinit_rpmsg' failed");
}

void tc_1_defchnl(void)
{
    for (int i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_1();
    }
    for (int i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_2();
    }
    for (int i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_3();
    }
    for (int i = 0; i < TC_INIT_COUNT; i++)
    {
        tc_1_defchnl_4();
    }
}

void run_tests(void *unused)
{
    RUN_TEST(tc_1_defchnl, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
