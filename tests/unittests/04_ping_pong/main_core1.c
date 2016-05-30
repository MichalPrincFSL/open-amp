#include <string.h>
#include "unity.h"
#include "openamp/open_amp.h"
#include "assert.h"
#include "pingpong_common.h"

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;
struct rpmsg_channel *new_chnl = NULL;
volatile int message_received = 0;
volatile int message_received_2 = 0;
CONTROL_MESSAGE control_msg = {0};
unsigned int trans_data = 0;

// default channel callback
void test_read_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt missed");
    control_msg = *((CONTROL_MESSAGE_PTR)data);
    message_received = 1;
}

// new channel callback
void new_test_read_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received_2, "interrupt missed");
    control_msg = *((CONTROL_MESSAGE_PTR)data);
    message_received_2 = 1;
}

// custom ept callback
void ept_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt missed");
    trans_data = *((unsigned int *)data);
    message_received = 1;
}

// channel was created
void test_channel_created(struct rpmsg_channel *rp_chnl)
{
    assert(app_chnl == NULL);
    app_chnl = rp_chnl;
}

// a new channel was created, not the default channel
void test_new_channel_created(struct rpmsg_channel *rp_chnl)
{
    assert(new_chnl == NULL);
    if (rp_chnl->state == RPMSG_CHNL_STATE_ACTIVE)
        new_chnl = rp_chnl;
}

// new channel is going to be destroyed
void ts_new_channel_deleted(struct rpmsg_channel *rp_chnl)
{
    new_chnl = NULL;
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
    int result = rpmsg_init(1, &rdev, test_channel_created, ts_channel_deleted, test_read_cb, RPMSG_MASTER);
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

// this test case is to test the endpoint functionality.
void tc_1_create_delete_ep_cmd_responder(void)
{
    void *mutex = NULL;
    int result = 0;
    struct rpmsg_endpoint *ept, *ept_to_delete;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM_PTR data_create_ep_param;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM_PTR data_destroy_ep_param;
    ACKNOWLEDGE_MESSAGE ack_msg;
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;
    result = env_create_mutex(&mutex, 1);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    // create an endpoint.
    /* wait for control message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting the create endpoint command");
    data_create_ep_param = env_allocate_memory(sizeof(struct control_message_data_create_ept_param));
    env_memcpy((void *)(data_create_ep_param), (void *)control_msg.DATA,
               sizeof(struct control_message_data_create_ept_param));
    ept = rpmsg_create_ept(app_chnl, ept_cb, NULL, data_create_ep_param->ept_to_create_addr);
    /* this is to send back the ack message */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
        ack_msg.RETURN_VALUE = (ept == NULL);
        ack_msg.RESP_DATA[0] = ept->addr;
        result = rpmsg_send_offchannel(app_chnl, app_chnl->src, data_create_ep_param->ept_to_ack_addr, &ack_msg,
                                       sizeof(struct acknowledge_message));
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        TEST_ASSERT_MESSAGE((ept != NULL ? 1 : (0 != ts_deinit_rpmsg())), "error! endpoint creation failed");

        /* wait for a message from sender to newly created endpoint */
        while (!message_received)
            ;
        TEST_ASSERT_MESSAGE((EP_SIGNATURE == trans_data ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! failed to receive from custom endpoint");
        trans_data = 0;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
    }

    // create another endpoint with same address as above.
    /* wait for control message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting the create endpoint command");
    env_memcpy((void *)(data_create_ep_param), (void *)control_msg.DATA,
               sizeof(struct control_message_data_create_ept_param));
    ept = NULL;
    ept = rpmsg_create_ept(app_chnl, ept_cb, NULL, data_create_ep_param->ept_to_create_addr);
    /* send the ack message if required */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
        ack_msg.RETURN_VALUE = (ept == NULL);
        result = rpmsg_send_offchannel(app_chnl, app_chnl->src, data_create_ep_param->ept_to_ack_addr, &ack_msg,
                                       sizeof(struct acknowledge_message));
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        TEST_ASSERT_MESSAGE((ept == NULL ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! creation of endpoint with similar address failed");
    }
    env_free_memory(data_create_ep_param);

    // delete endpoint
    data_destroy_ep_param = env_allocate_memory(sizeof(struct control_message_data_destroy_ept_param));
    for (int i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_DESTROY_EP ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting the destroy endpoint command");
        env_memcpy((void *)(data_destroy_ep_param), (void *)control_msg.DATA,
                   sizeof(struct control_message_data_destroy_ept_param));
        struct llist *node;
        node = rpmsg_rdev_get_endpoint_from_addr(rdev, data_destroy_ep_param->ept_to_destroy_addr);
        if (node != NULL)
        {
            ept_to_delete = (struct rpmsg_endpoint *)node->data;
            rpmsg_destroy_ept(ept_to_delete);
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_DESTROY_EP;
            ack_msg.RETURN_VALUE = (node == NULL);
            result = rpmsg_sendto(app_chnl, &ack_msg, sizeof(struct acknowledge_message),
                                  data_destroy_ep_param->ept_to_ack_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }
    env_free_memory(data_destroy_ep_param);

    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

// this test case is to test the send functionality.
void tc_2_send_cmd_responder(void)
{
    void *mutex = NULL, *data_addr = NULL;
    int result = 0;
    unsigned long buf_size;
    CONTROL_MESSAGE_DATA_SEND_PARAM_PTR data_send_param;
    char data[4] = "cba";
    char data_2[11] = "cba nocopy";
    ACKNOWLEDGE_MESSAGE ack_msg;
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;
    result = env_create_mutex(&mutex, 1);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    data_send_param = env_allocate_memory(sizeof(struct control_message_data_send_param));
    // execute the send command
    for (int i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_SEND ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting the send command");
        env_memcpy((void *)(data_send_param), (void *)control_msg.DATA, sizeof(struct control_message_data_send_param));
        if (env_strncmp(data_send_param->msg, "", 1) == 0)
        {
            result = rpmsg_sendto(app_chnl, data, 4, data_send_param->dest_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
        }
        else
        {
            result =
                rpmsg_sendto(app_chnl, data_send_param->msg, data_send_param->msg_size, data_send_param->dest_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_SEND;
            ack_msg.RETURN_VALUE = result;
            result =
                rpmsg_sendto(app_chnl, &ack_msg, sizeof(struct acknowledge_message), data_send_param->ept_to_ack_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }

    // execute the send nocopy command
    for (int i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_SEND_NO_COPY ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting the send nocopy command");
        env_memcpy((void *)(data_send_param), (void *)control_msg.DATA, sizeof(struct control_message_data_send_param));
        data_addr = rpmsg_alloc_tx_buffer(app_chnl, &buf_size, RPMSG_TRUE);
        TEST_ASSERT_MESSAGE((NULL != data_addr ? 1 : (0 != ts_deinit_rpmsg())), "error! no more buffer available");
        if (env_strncmp(data_send_param->msg, "", 1) == 0)
        {
            env_memcpy(data_addr, data_2, 11);
            result = rpmsg_sendto_nocopy(app_chnl, data_addr, 11, data_send_param->dest_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
            data_addr = NULL;
        }
        else
        {
            env_memcpy(data_addr, data_send_param->msg, data_send_param->msg_size);
            result = rpmsg_sendto_nocopy(app_chnl, data_addr, data_send_param->msg_size, data_send_param->dest_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
            data_addr = NULL;
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_SEND_NO_COPY;
            ack_msg.RETURN_VALUE = result;
            result =
                rpmsg_sendto(app_chnl, &ack_msg, sizeof(struct acknowledge_message), data_send_param->ept_to_ack_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }

    env_free_memory(data_send_param);
    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

// this test case is to test the channel creation/destruction functionality.
void tc_3_create_delete_channel_cmd_responder(void)
{
    void *mutex = NULL;
    int result = 0;
    struct rpmsg_channel *rpmsg_chanl;
    ACKNOWLEDGE_MESSAGE ack_msg;
    CONTROL_MESSAGE_DATA_CREATE_CHANNEL_PARAM_PTR data_create_channel_param;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM_PTR data_create_ep_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM_PTR data_send_param;
    CONTROL_MESSAGE_DATA_DELETE_CHANNEL_PARAM_PTR data_delete_channel_param;
    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;
    result = env_create_mutex(&mutex, 1);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");
    env_lock_mutex(mutex);
    message_received = 0;
    message_received_2 = 0;
    env_unlock_mutex(mutex);

    data_create_channel_param = env_allocate_memory(sizeof(struct control_message_data_create_channel_param));
    /* wait for control message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_CREATE_CHANNEL ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting create channel command");
    env_memcpy((void *)(data_create_channel_param), (void *)control_msg.DATA,
               sizeof(struct control_message_data_create_channel_param));
    rdev->channel_created = test_new_channel_created;
    rdev->default_cb = new_test_read_cb;
    rdev->channel_destroyed = ts_new_channel_deleted;
    rpmsg_chanl = rpmsg_create_channel(rdev, data_create_channel_param->name);
    while (new_chnl == NULL)
        ;
    /* send the ack message if required */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_CREATE_CHANNEL;
        ack_msg.RETURN_VALUE = (rpmsg_chanl == NULL);
        env_memcpy((void *)ack_msg.RESP_DATA, (void *)rpmsg_chanl, sizeof(struct rpmsg_channel));
        result = rpmsg_sendto(app_chnl, &ack_msg, sizeof(struct acknowledge_message),
                              data_create_channel_param->ep_to_ack_addr);
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
    }
    env_free_memory(data_create_channel_param);

    // create two endpoints with the same address on two different channels.
    /* wait for control message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    data_create_ep_param = env_allocate_memory(sizeof(struct control_message_data_create_ept_param));
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting create endpoint command");
    env_memcpy((void *)(data_create_ep_param), (void *)control_msg.DATA,
               sizeof(struct control_message_data_create_ept_param));
    struct rpmsg_endpoint *ept, *ept_2;
    ept = rpmsg_create_ept(app_chnl, ept_cb, NULL, data_create_ep_param->ept_to_create_addr);
    ept_2 = rpmsg_create_ept(new_chnl, ept_cb, NULL, data_create_ep_param->ept_to_create_addr);
    /* send the ack message if required */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
        ack_msg.RETURN_VALUE = (ept != NULL) && (ept_2 == NULL);
        result =
            rpmsg_sendto(app_chnl, &ack_msg, sizeof(struct acknowledge_message), data_create_ep_param->ept_to_ack_addr);
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
    }
    env_free_memory(data_create_ep_param);
    rpmsg_destroy_ept(ept);

    // send message between endpoints in default channel and new channel.
    /* wait for control message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    data_send_param = env_allocate_memory(sizeof(struct control_message_data_send_param));
    TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_SEND ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting send command");
    env_memcpy((void *)(data_send_param), (void *)control_msg.DATA, sizeof(struct control_message_data_send_param));
    result =
        rpmsg_sendto(app_chnl, (void *)data_send_param->msg, data_send_param->msg_size, data_send_param->dest_addr);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* send the ack message if required */
    if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
    {
        memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
        ack_msg.CMD_ACK = CTR_CMD_SEND;
        ack_msg.RETURN_VALUE = result;
        result = rpmsg_sendto(new_chnl, &ack_msg, sizeof(struct acknowledge_message), data_send_param->ept_to_ack_addr);
        TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
    }
    env_free_memory(data_send_param);

    // delete channel
    data_delete_channel_param = env_allocate_memory(sizeof(struct control_message_data_delete_channel_param));
    for (int i = 0; i < 2; i++)
    {
        /* wait for control message */
        while (!message_received)
            ;
        env_lock_mutex(mutex);
        message_received = 0;
        env_unlock_mutex(mutex);
        TEST_ASSERT_MESSAGE((control_msg.CMD == CTR_CMD_DESTROY_CHANNEL ? 1 : (0 != ts_deinit_rpmsg())),
                            "error! expecting destroy channel command");
        env_memcpy((void *)(data_delete_channel_param), (void *)control_msg.DATA,
                   sizeof(struct control_message_data_delete_channel_param));
        struct llist *node;
        rpmsg_chanl = NULL;
        node = rpmsg_rdev_get_chnl_node_from_id(rdev, (char *)data_delete_channel_param->name);
        if (node != NULL)
        {
            rpmsg_chanl = (struct rpmsg_channel *)node->data;
            rpmsg_delete_channel(rpmsg_chanl);
            while (new_chnl != NULL)
                ;
        }
        /* send the ack message if required */
        if (control_msg.ACK_REQUIRED == ACK_REQUIRED_YES)
        {
            memset(&ack_msg, 0, sizeof(ACKNOWLEDGE_MESSAGE));
            ack_msg.CMD_ACK = CTR_CMD_DESTROY_CHANNEL;
            ack_msg.RETURN_VALUE = (new_chnl != NULL);
            result = rpmsg_sendto(app_chnl, &ack_msg, sizeof(struct acknowledge_message),
                                  data_delete_channel_param->ep_to_ack_addr);
            TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send acknowledge message failed");
        }
    }
    env_free_memory(data_delete_channel_param);

    rdev->channel_created = test_channel_created;
    rdev->default_cb = test_read_cb;
    rdev->channel_destroyed = ts_channel_deleted;
    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "error! system clean up error");
}

void run_tests(void *unused)
{
    RUN_TEST(tc_1_create_delete_ep_cmd_responder, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_2_send_cmd_responder, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    RUN_TEST(tc_3_create_delete_channel_cmd_responder, MAKE_UNITY_NUM(k_unity_rpmsg, 2));
}

/* EOF */
