#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "openamp/open_amp.h"
#include "openamp/rpmsg_rtos.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"

#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;

// utility: initialize rpmsg and enviroment
// and wait for default channel
int ts_init_rpmsg(void)
{
    env_init();
    int result = rpmsg_rtos_init(0, &rdev, RPMSG_MASTER, &app_chnl);
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

int pattern_cmp(char *buffer, char pattern, int len)
{
    for (int i = 0; i < len; i++)
        if (buffer[i] != pattern)
            return -1;
    return 0;
}

/******************************************************************************
 * Test case 1
 * - verify simple transport between default epts of default channels
 * - verify simple nocopy transport between default epts of default channels
 * - verify simple transport between custom created epts of default channels
 * - verify simple nocopy transport between custom created epts of default channels
 *****************************************************************************/
void tc_1_receive_send(void)
{
    int result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int len = 0;
        unsigned long int src = 0;
        result = rpmsg_rtos_recv(app_chnl->rp_ept, data, &len, DATA_LEN, &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int len = 0;
        unsigned long int src = 0;
        result = rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, &data_addr, &len, &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data_addr, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = rpmsg_rtos_release_rx_buffer(app_chnl->rp_ept, data_addr);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        memset(data, i, DATA_LEN);
        result = rpmsg_rtos_send(app_chnl->rp_ept, data, DATA_LEN, app_chnl->dst);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        memset(data, i, DATA_LEN);
        result = rpmsg_rtos_send(app_chnl->rp_ept, data, DATA_LEN, app_chnl->dst);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    // invalid params for receive
    result = rpmsg_rtos_recv(NULL, data, NULL, DATA_LEN, NULL, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_rtos_recv(app_chnl->rp_ept, NULL, NULL, DATA_LEN, NULL, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_rtos_recv(app_chnl->rp_ept, NULL, NULL, 0xFFFFFFFF, NULL, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");

    // invalid params for receive_nocopy
    result = rpmsg_rtos_recv_nocopy(NULL, &data_addr, NULL, NULL, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, NULL, NULL, NULL, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");

    // invalid params for receive_nocopy_free
    result = rpmsg_rtos_release_rx_buffer(NULL, &data_addr);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_rtos_release_rx_buffer(app_chnl->rp_ept, NULL);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
}

/******************************************************************************
 * Test case 2
 * - same as for test case 1 but send with copy replaced by send_nocopy
 *****************************************************************************/
void tc_2_receive_send(void)
{
    int result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    unsigned long buf_size = 0;

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int len = 0;
        unsigned long int src = 0;
        result = rpmsg_rtos_recv(app_chnl->rp_ept, data, &len, DATA_LEN, &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        int len = 0;
        unsigned long int src = 0;
        result = rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, &data_addr, &len, &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = pattern_cmp(data_addr, i, DATA_LEN);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        result = rpmsg_rtos_release_rx_buffer(app_chnl->rp_ept, data_addr);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
    }

    data_addr = NULL;
    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        data_addr = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, &buf_size);
        TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
        TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
        memset(data_addr, i, DATA_LEN);
        result = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, data_addr, DATA_LEN, app_chnl->dst);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        data_addr = NULL;
    }

    for (int i = 0; i < TC_TRANSFER_COUNT; i++)
    {
        data_addr = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, &buf_size);
        TEST_ASSERT_MESSAGE(NULL != data_addr, "negative number");
        TEST_ASSERT_MESSAGE(0 != buf_size, "negative number");
        memset(data_addr, i, DATA_LEN);
        result = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, data_addr, DATA_LEN, app_chnl->dst);
        TEST_ASSERT_MESSAGE(0 == result, "negative number");
        data_addr = NULL;
    }

    // invalid params for alloc_tx_buffer
    data_addr = rpmsg_rtos_alloc_tx_buffer(NULL, &buf_size);
    TEST_ASSERT_MESSAGE(NULL == data_addr, "negative number");
    data_addr = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, NULL);
    TEST_ASSERT_MESSAGE(NULL == data_addr, "negative number");

    // invalid params for send_nocopy
    result = rpmsg_rtos_send_nocopy(NULL, data_addr, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, NULL, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
    result = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, data_addr, 0xFFFFFFFF, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "negative number");
}

void run_tests(void *unused)
{
    int result = 0;
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    if (!result)
    {
        RUN_TEST(tc_1_receive_send, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
        RUN_TEST(tc_2_receive_send, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    }
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
}
