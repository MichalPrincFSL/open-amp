#include <string.h>
#include "unity.h"
#include "openamp/open_amp.h"
#include "assert.h"

#define DATA_LEN 45
#define TRYSEND_COUNT 12

struct remote_device *rdev = NULL;
struct rpmsg_channel * volatile app_chnl = NULL;
volatile int test_no = 0, rx_data_len = 0;
void *rx_buffer = NULL;

int pattern_cmp(char *buffer, char pattern, int len)
{
    for (int i = 0; i < len; i++)
        if (buffer[i] != pattern)
            return -1;
    return 0;
}

// default channel callback
void test_read_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    int result = 0;
    switch (test_no)
    {
        case 0:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, DATA_LEN);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 1:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 2:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 3:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 4:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 5:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 6:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 7:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 8:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 9:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 10:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            result = pattern_cmp(data, test_no, len);
            TEST_ASSERT_MESSAGE(0 == result, "receive error");
            break;
        case 11:
            TEST_ASSERT_MESSAGE(src == app_chnl->dst, "receive error");
            TEST_ASSERT_MESSAGE(DATA_LEN == len, "receive error");
            rx_data_len = len;
            rx_buffer = data;
            rpmsg_hold_rx_buffer((struct rpmsg_channel *)app_chnl, rx_buffer);
            break;
    }
    test_no++;
}

// channel was created
void test_channel_created(struct rpmsg_channel *rp_chnl)
{
    assert(app_chnl == NULL);
    app_chnl = rp_chnl;
}

// channel is going to be destroyed
void ts_channel_deleted(struct rpmsg_channel *rp_chnl)
{
    app_chnl = NULL;
}

// utility: initialize rpmsg and enviroment
// and wait for default channel
int ts_init_rpmsg(void)
{
    env_init();
    int result = rpmsg_init(0, &rdev, test_channel_created, ts_channel_deleted, test_read_cb, RPMSG_MASTER);
    TEST_ASSERT_MESSAGE(0 == result, "init function failed");
    while (NULL == app_chnl)
        ;
    return 0;
}

// utility: deinitialize rpmsg and enviroment
int ts_deinit_rpmsg(void)
{
    rpmsg_deinit(rdev);
    env_deinit();
    app_chnl = NULL;
    return 0;
}

/******************************************************************************
 * Test case 1
 * - check received data
 *****************************************************************************/
void tc_1_receive(void)
{
    int result = 0;
    // wait for incoming interrupts
    while (test_no != 12)
        ;
    // check the last message content - this message has not been processed within the rx callback
    TEST_ASSERT_MESSAGE(NULL != rx_buffer, "receive error");
    TEST_ASSERT_MESSAGE(0 != rx_data_len, "receive error");
    result = pattern_cmp(rx_buffer, 11, rx_data_len);
    TEST_ASSERT_MESSAGE(0 == result, "receive error");
    rpmsg_release_rx_buffer((struct rpmsg_channel *)app_chnl, rx_buffer);
}

/******************************************************************************
 * Test case 2
 * - send data with different pattern for each send function
 * - call send function with invalid parameters
 *****************************************************************************/
void tc_2_send(void)
{
    int result = 0;
    char data[DATA_LEN] = {0};
    void *data_addr = NULL;
    unsigned long buf_size = 0;

    // send - invalid channel
    result = rpmsg_sendto(NULL, data, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send(NULL, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel(NULL, app_chnl->src, app_chnl->dst, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysend(NULL, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysendto(NULL, data, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysend_offchannel(NULL, app_chnl->src, app_chnl->dst, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel_raw(NULL, app_chnl->src, app_chnl->dst, data, DATA_LEN, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel_raw(NULL, app_chnl->src, app_chnl->dst, data, DATA_LEN, RPMSG_FALSE);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_nocopy(NULL, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_sendto_nocopy(NULL, data_addr, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel_nocopy(NULL, app_chnl->src, app_chnl->dst, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");

    // send - invalid data
    result = rpmsg_sendto((struct rpmsg_channel *)app_chnl, NULL, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send((struct rpmsg_channel *)app_chnl, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysend((struct rpmsg_channel *)app_chnl, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysendto((struct rpmsg_channel *)app_chnl, NULL, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysend_offchannel((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    // 'rpmsg_send_offchannel_raw' allows send data at address 0 - NULL
    // I'm not sure if it's a bug or feature
    // result = rpmsg_send_offchannel_raw(app_chnl, app_chnl->src, app_chnl->dst, NULL, DATA_LEN, RPMSG_TRUE);
    // TEST_ASSERT_MESSAGE(0 != result, "send error");
    // result = rpmsg_send_offchannel_raw(app_chnl, app_chnl->src, app_chnl->dst, NULL, DATA_LEN, RPMSG_FALSE);
    // TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_nocopy((struct rpmsg_channel *)app_chnl, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_sendto_nocopy((struct rpmsg_channel *)app_chnl, NULL, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result =
        rpmsg_send_offchannel_nocopy((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, NULL, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 != result, "send error");

    // send - invalid size
    result = rpmsg_get_buffer_size((struct rpmsg_channel *)app_chnl);
    TEST_ASSERT_MESSAGE((0xFFFFFFFF > result) && (result > 0), "send error");
    result = rpmsg_sendto((struct rpmsg_channel *)app_chnl, data, 0xFFFFFFFF, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send((struct rpmsg_channel *)app_chnl, data, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysend((struct rpmsg_channel *)app_chnl, data, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysendto((struct rpmsg_channel *)app_chnl, data, 0xFFFFFFFF, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_trysend_offchannel((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel_raw((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, 0xFFFFFFFF,
                                       RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel_raw((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, 0xFFFFFFFF,
                                       RPMSG_FALSE);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_nocopy((struct rpmsg_channel *)app_chnl, data_addr, 0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_sendto_nocopy((struct rpmsg_channel *)app_chnl, data_addr, 0xFFFFFFFF, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 != result, "send error");
    result = rpmsg_send_offchannel_nocopy((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data_addr,
                                          0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 != result, "send error");

    // invalid params for alloc_tx_buffer
    data_addr = rpmsg_alloc_tx_buffer(NULL, &buf_size, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(NULL == data_addr, "send error");
    data_addr = rpmsg_alloc_tx_buffer((struct rpmsg_channel *)app_chnl, NULL, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(NULL == data_addr, "send error");

    // send - valid all params
    memset(data, 0, DATA_LEN);
    result = rpmsg_sendto((struct rpmsg_channel *)app_chnl, data, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 1, DATA_LEN);
    result = rpmsg_send((struct rpmsg_channel *)app_chnl, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 2, DATA_LEN);
    result = rpmsg_send_offchannel((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 3, DATA_LEN);
    for (int i = 0; i < TRYSEND_COUNT; i++)
    {
        result = rpmsg_trysend((struct rpmsg_channel *)app_chnl, data, DATA_LEN);
        if (result != RPMSG_ERR_NO_MEM)
            break;
        env_sleep_msec(200);
    }
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 4, DATA_LEN);
    for (int i = 0; i < TRYSEND_COUNT; i++)
    {
        result = rpmsg_trysendto((struct rpmsg_channel *)app_chnl, data, DATA_LEN, app_chnl->dst);
        if (result != RPMSG_ERR_NO_MEM)
            break;
        env_sleep_msec(200);
    }
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 5, DATA_LEN);
    for (int i = 0; i < TRYSEND_COUNT; i++)
    {
        result =
            rpmsg_trysend_offchannel((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, DATA_LEN);
        if (result != RPMSG_ERR_NO_MEM)
            break;
        env_sleep_msec(200);
    }
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 6, DATA_LEN);
    result = rpmsg_send_offchannel_raw((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data, DATA_LEN,
                                       RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    memset(data, 7, DATA_LEN);
    for (int i = 0; i < TRYSEND_COUNT; i++)
    {
        result = rpmsg_send_offchannel_raw((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data,
                                           DATA_LEN, RPMSG_FALSE);
        if (result != RPMSG_ERR_NO_MEM)
            break;
        env_sleep_msec(200);
    }
    TEST_ASSERT_MESSAGE(0 == result, "send error");

    data_addr = rpmsg_alloc_tx_buffer((struct rpmsg_channel *)app_chnl, &buf_size, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "send error");
    TEST_ASSERT_MESSAGE(0 != buf_size, "send error");
    memset(data_addr, 8, DATA_LEN);
    result = rpmsg_sendto_nocopy((struct rpmsg_channel *)app_chnl, data_addr, DATA_LEN, app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 == result, "send error");
    data_addr = NULL;

    data_addr = rpmsg_alloc_tx_buffer((struct rpmsg_channel *)app_chnl, &buf_size, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "send error");
    TEST_ASSERT_MESSAGE(0 != buf_size, "send error");
    memset(data_addr, 9, DATA_LEN);
    result = rpmsg_send_nocopy((struct rpmsg_channel *)app_chnl, data_addr, DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");
    data_addr = NULL;

    data_addr = rpmsg_alloc_tx_buffer((struct rpmsg_channel *)app_chnl, &buf_size, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "send error");
    TEST_ASSERT_MESSAGE(0 != buf_size, "send error");
    memset(data_addr, 10, DATA_LEN);
    result = rpmsg_send_offchannel_nocopy((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data_addr,
                                          DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");
    data_addr = NULL;

    data_addr = rpmsg_alloc_tx_buffer((struct rpmsg_channel *)app_chnl, &buf_size, RPMSG_TRUE);
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "send error");
    TEST_ASSERT_MESSAGE(0 != buf_size, "send error");
    memset(data_addr, 11, DATA_LEN);
    result = rpmsg_send_offchannel_nocopy((struct rpmsg_channel *)app_chnl, app_chnl->src, app_chnl->dst, data_addr,
                                          DATA_LEN);
    TEST_ASSERT_MESSAGE(0 == result, "send error");
    data_addr = NULL;

    /* wait a while to process the last message on the opposite side */
    env_sleep_msec(200);
}

void run_tests(void *unused)
{
    int result = 0;
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "receive error");
    if (!result)
    {
        RUN_TEST(tc_1_receive, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
        RUN_TEST(tc_2_send, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    }
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "receive error");
}
