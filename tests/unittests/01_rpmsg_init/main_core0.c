#include "unity.h"
#include "openamp/open_amp.h"
#include "assert.h"

enum test_state
{
    state_default = 0,
    state_created_channel,
    state_deleted_channel,
    state_read_data,
    state_channel_null,
    state_channel_name_null,
    state_channel_rdev_null,
    state_channel_ept_null,
    state_channel_ept_addr,
    state_channel_ept_cb
};

int test_state = state_default;
struct remote_device *rdev = NULL;
struct rpmsg_channel * volatile app_chnl = NULL;
int test_counter = 0;

void test_read_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    // change state only if has default value
    if (state_default == test_state)
        test_state = state_read_data;
}

void test_channel_created(struct rpmsg_channel *rp_chnl)
{
    assert(app_chnl == NULL);
    app_chnl = rp_chnl;
    // perform check *only* if state is *default*
    while (state_default == test_state)
    {
        // check state of passed parameter 'rp_chnl'
        test_state = state_created_channel;
        // check channel ptr
        if (rp_chnl == NULL)
        {
            test_state = state_channel_null;
            break;
        }
        // check channel name
        if (rp_chnl->name == NULL)
        {
            test_state = state_channel_name_null;
            break;
        }
        // check channel device
        if (rp_chnl->rdev == NULL)
        {
            test_state = state_channel_rdev_null;
            break;
        }
        // check channel default ept
        if (rp_chnl->rp_ept == NULL)
        {
            test_state = state_channel_ept_null;
            break;
        }
        // channel uses address of default ept
        if (rp_chnl->rp_ept->addr != rp_chnl->src)
        {
            test_state = state_channel_ept_addr;
            break;
        }
        // default ept callback must point to read callback in init
        if (test_read_cb != rp_chnl->rp_ept->cb)
        {
            test_state = state_channel_ept_cb;
            break;
        }
    }
}

void test_channel_deleted(struct rpmsg_channel *rp_chnl)
{
    // perform check *only* if state is *default*
    while (state_default == test_state)
    {
        // check state of passed parameter 'rp_chnl'
        test_state = state_deleted_channel;
        // check channel ptr
        if (rp_chnl == NULL)
        {
            test_state = state_channel_null;
            break;
        }
        // check channel name
        if (rp_chnl->name == NULL)
        {
            test_state = state_channel_name_null;
            break;
        }
        // check channel device
        if (rp_chnl->rdev == NULL)
        {
            test_state = state_channel_rdev_null;
            break;
        }
        // check channel default ept
        if (rp_chnl->rp_ept == NULL)
        {
            test_state = state_channel_ept_null;
            break;
        }
        // channel uses address of default ept
        if (rp_chnl->rp_ept->addr != rp_chnl->src)
        {
            test_state = state_channel_ept_addr;
            break;
        }
        // default ept callback must point to read callback in init
        if (test_read_cb != rp_chnl->rp_ept->cb)
        {
            test_state = state_channel_ept_cb;
            break;
        }
    }
    app_chnl = NULL;
}

void tc_1_rpmsg_init()
{
    int result;

    for (test_counter = 0; test_counter < 200; test_counter++)
    {
        env_init();

        result = rpmsg_init(1, &rdev, test_channel_created, test_channel_deleted, test_read_cb, RPMSG_REMOTE);
        TEST_ASSERT_MESSAGE(0 == result, "init function failed");

        /* incomming interrupt changes state to state_created_channel */
        while (NULL == app_chnl)
            ;
        TEST_ASSERT_MESSAGE(test_state == state_created_channel, "chanel should be already created");
        test_state = state_default;

        rpmsg_deinit(rdev);
        env_deinit();

        /* incomming interrupt changes state to state_created_channel */
        while (NULL != app_chnl)
            ;
        TEST_ASSERT_MESSAGE(test_state == state_deleted_channel, "chanel should be already deleted");
        test_state = state_default;
    }
}

void run_tests()
{
    RUN_TEST(tc_1_rpmsg_init, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
