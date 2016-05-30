#include "unity.h"
#include "openamp/open_amp.h"
#include "assert.h"

#define TC_INIT_COUNT 24
#define TC_TRANSFER_COUNT 10
#define TC_EPT_COUNT 3
#define TC_CHANNEL_COUNT 2

struct remote_device *rdev = NULL;
struct rpmsg_channel * volatile app_chnl = NULL;
volatile int message_received = 0;
int trans_data = 0;

// default channel callback
void test_read_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt miss");
    trans_data = *((int *)data);
    message_received = 1;
}

// custom ept callback
void ept_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt miss");
    trans_data = *((int *)data);
    message_received = 1;
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

// utility: create number of epts
int ts_create_epts(struct rpmsg_channel *channel, struct rpmsg_endpoint *epts[], int count, int init_addr)
{
    TEST_ASSERT_MESSAGE(channel != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(epts != NULL, "NULL param");
    TEST_ASSERT_MESSAGE(count > 0, "negative number");
    for (int i = 0; i < count; i++)
    {
        epts[i] = rpmsg_create_ept(channel, ept_cb, NULL, init_addr + i);
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
        rpmsg_destroy_ept(epts[i]);
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
 * - verify create/delete/recreation of epts with same address
 * - verify simple transport between custom created epts of default channels
 *****************************************************************************/
void tc_1_defchnl_transport_receive(
    struct rpmsg_channel *channel, int src, int dst, int reply_value, int expected_value)
{
    void *mutex = NULL;
    int result = 0;
    result = env_create_mutex(&mutex, 1);
    TEST_ASSERT_MESSAGE(0 == result, "unexpected value");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    TEST_ASSERT_MESSAGE(trans_data == expected_value, "unexpected value");
    result = rpmsg_send_offchannel(channel, src, dst, &reply_value, sizeof(trans_data));
    TEST_ASSERT_MESSAGE(0 == result, "send failed");
    message_received = 0;
    env_unlock_mutex(mutex);
    env_delete_mutex(mutex);
}

void tc_1_defchnl_transport(void)
{
    struct rpmsg_endpoint *epts[TC_EPT_COUNT] = {0};
    int result = 0;

    // init rpmsg and enviroment, check nonrecoverable error
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "'ts_init_rpmsg' failed");
    if (result)
        goto end0;

    // transfer data between default epts
    for (int j = 0; j < TC_TRANSFER_COUNT; j++)
    {
        // send data to default channel on REMOTE
        tc_1_defchnl_transport_receive((struct rpmsg_channel *)app_chnl,
                                       app_chnl->src,      // src addr
                                       app_chnl->dst,      // dst addr
                                       app_chnl->dst + 10, // value to send
                                       app_chnl->dst       // expected value to receive
                                       );
    }

    // create custom epts of default channel, check nonrecoverable error
    result = ts_create_epts((struct rpmsg_channel *)app_chnl, epts, TC_EPT_COUNT, 10);
    TEST_ASSERT_MESSAGE(0 == result, "'ts_create_epts' failed");
    if (result)
        goto end1;

    // transfer data through custom epts
    for (int j = 0; j < TC_TRANSFER_COUNT; j++)
    {
        for (int i = 0; i < TC_EPT_COUNT; i++)
        {
            // remote has custom epts with the same *address*
            // send data between custom epts with same *address*
            tc_1_defchnl_transport_receive((struct rpmsg_channel *)app_chnl,
                                           epts[i]->addr,      // src addr
                                           epts[i]->addr,      // dst addr
                                           epts[i]->addr + 10, // value to reply
                                           epts[i]->addr       // expected value to receive
                                           );
            // send data from custom epts to REMOTE default channel ept
            tc_1_defchnl_transport_receive((struct rpmsg_channel *)app_chnl,
                                           app_chnl->src,      // src addr
                                           epts[i]->addr,      // dst addr
                                           epts[i]->addr + 10, // value to reply
                                           epts[i]->addr       // expected value to receive
                                           );
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
        tc_1_defchnl_transport();
    }
}

void run_tests(void *unused)
{
    RUN_TEST(tc_1_defchnl, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
