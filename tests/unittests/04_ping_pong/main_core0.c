#include <string.h>
#include "unity.h"
#include "openamp/open_amp.h"
#include "assert.h"
#include "pingpong_common.h"

/* This is for the responder side */

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;
struct rpmsg_channel *new_chnl = NULL;
volatile int message_received = 0;
volatile int message_received_2 = 0;
ACKNOWLEDGE_MESSAGE ack_msg = {0};
char trans_data[100] = {0};

// default channel callback
void test_read_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt missed");
    ack_msg = *((ACKNOWLEDGE_MESSAGE_PTR)data);
    message_received = 1;
}

// custom ept callback
void ept_cb(struct rpmsg_channel *rp_chnl, void *data, int len, void *priv, unsigned long src)
{
    TEST_ASSERT_MESSAGE(0 == message_received, "interrupt missed");
    env_memcpy((void *)trans_data, data, len);
    message_received_2 = 1;
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
    env_sleep_msec(RPROC_BOOT_DELAY);
    int result = rpmsg_init(1, &rdev, test_channel_created, ts_channel_deleted, test_read_cb, RPMSG_REMOTE);
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
void tc_1_create_delete_ep_cmd_sender(void)
{
    void *mutex = NULL;
    int result = 0;
    unsigned int data = EP_SIGNATURE;
    unsigned long dest = 0;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ep_param;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ep_param;
    CONTROL_MESSAGE control_msg = {0};

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to init");
    if (result)
        goto end;
    result = env_create_mutex(&mutex, 1);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! failed to create mutex");
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    // create ep command
    data_create_ep_param.ept_to_ack_addr = app_chnl->src;
    data_create_ep_param.ept_to_create_addr = RPMSG_ADDR_ANY;
    control_msg.CMD = CTR_CMD_CREATE_EP;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_create_ep_param),
               sizeof(struct control_message_data_create_ept_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);

    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of create endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! create endpoint command operation in other side failed");
    dest = ack_msg.RESP_DATA[0];
    /* send message to newly created endpoint */
    result = rpmsg_sendto(app_chnl, (void *)(&data), sizeof(unsigned int), dest);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");

    // create another endpoint with same address
    data_create_ep_param.ept_to_create_addr = dest;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_create_ep_param),
               sizeof(struct control_message_data_create_ept_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of create endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE != 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! create new endpoint with same address as another endpoint in the other side failed");

    // delete created endpoint
    /* destroy endpoint not created yet */
    data_destroy_ep_param.ept_to_ack_addr = app_chnl->src;
    data_destroy_ep_param.ept_to_destroy_addr = dest + 1;
    control_msg.CMD = CTR_CMD_DESTROY_EP;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_destroy_ep_param),
               sizeof(struct control_message_data_destroy_ept_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_DESTROY_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of destroy endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE != 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! destroy an unexisted endpoint failed");
    /* destroy endpoint created before */
    data_destroy_ep_param.ept_to_destroy_addr = dest;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_destroy_ep_param),
               sizeof(struct control_message_data_destroy_ept_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_DESTROY_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of destroy endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! destroy an endpoint failed");

    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

// this test case is to test the send functionality.
void tc_2_send_cmd_sender(void)
{
    void *mutex = NULL;
    int result = 0;
    char data[10] = "abc";
    char data_2[15] = "abc nocopy";
    struct rpmsg_endpoint *ept;
    CONTROL_MESSAGE control_msg = {0};
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param = {0};

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

    // this custom endpoint is to receive the data from responder.
    ept = rpmsg_create_ept(app_chnl, ept_cb, NULL, RPMSG_ADDR_ANY);
    TEST_ASSERT_MESSAGE((NULL != ept ? 1 : (0 != ts_deinit_rpmsg())), "error! creation of an endpoint failed");

    // send command
    // send message "abc" and get back message "abc"
    data_send_param.dest_addr = ept->addr;
    data_send_param.ept_to_ack_addr = app_chnl->src;
    env_memcpy((void *)data_send_param.msg, (void *)data, 4);
    data_send_param.msg_size = 4;
    control_msg.CMD = CTR_CMD_SEND;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_SEND ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of send command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! send operation in the other side failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, data, 3) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // send an empty message "" and get back "cba"
    control_msg.ACK_REQUIRED = ACK_REQUIRED_NO;
    data_send_param.msg[0] = '\0';
    data_send_param.msg_size = 1;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, "cba", 3) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // send nocopy command
    // send message "abc nocopy" and get back message "abc nocopy"
    data_send_param.dest_addr = ept->addr;
    data_send_param.ept_to_ack_addr = app_chnl->src;
    env_memcpy((void *)data_send_param.msg, (void *)data_2, 11);
    data_send_param.msg_size = 11;
    control_msg.CMD = CTR_CMD_SEND_NO_COPY;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_SEND_NO_COPY ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of send nocopy command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! send operation in the other side failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, data, 3) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // send an empty message "" and get back "cba nocopy"
    control_msg.ACK_REQUIRED = ACK_REQUIRED_NO;
    data_send_param.msg[0] = '\0';
    data_send_param.msg_size = 1;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, "cba nocopy", 10) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    rpmsg_destroy_ept(ept);
    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

// this test case is to test the channel creation/destruction functionality.
void tc_3_create_delete_channel_cmd_sender(void)
{
    void *mutex = NULL;
    int result = 0;
    char name[32] = "test channel name";
    char data[5] = "adho";
    struct rpmsg_channel *rpmsg_chnl;
    CONTROL_MESSAGE control_msg = {0};
    CONTROL_MESSAGE_DATA_CREATE_CHANNEL_PARAM data_create_channel_param = {0};
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ep_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    CONTROL_MESSAGE_DATA_DELETE_CHANNEL_PARAM data_delete_channel_param;
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

    // create a new channel
    env_memcpy((void *)data_create_channel_param.name, (void *)name, sizeof(name));
    data_create_channel_param.ep_to_ack_addr = app_chnl->src;
    control_msg.CMD = CTR_CMD_CREATE_CHANNEL;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_create_channel_param),
               sizeof(struct control_message_data_send_param));
    rdev->channel_created = test_new_channel_created;
    rdev->default_cb = ept_cb;
    rdev->channel_destroyed = ts_new_channel_deleted;
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    while (new_chnl == NULL)
        ;
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_CREATE_CHANNEL ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of create channel command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! channel creation in the other side failed");
    rpmsg_chnl = env_allocate_memory(sizeof(struct rpmsg_channel));
    env_memcpy((void *)rpmsg_chnl, (void *)ack_msg.RESP_DATA, sizeof(struct rpmsg_channel));
    TEST_ASSERT_MESSAGE(
        (0 == env_strncmp((void *)rpmsg_chnl->name, (void *)name, sizeof(name)) ? 1 : (0 != ts_deinit_rpmsg())),
        "error! incorrect channel name");
    TEST_ASSERT_MESSAGE((rpmsg_chnl->src == new_chnl->dst ? 1 : (0 != ts_deinit_rpmsg())), "error! incorrect address");
    TEST_ASSERT_MESSAGE((rpmsg_chnl->dst == new_chnl->src ? 1 : (0 != ts_deinit_rpmsg())), "error! incorrect address");
    env_free_memory(rpmsg_chnl);

    // create 2 endpoints with the same address on 2 different channels at the responder side.
    data_create_ep_param.ept_to_ack_addr = app_chnl->src;
    data_create_ep_param.ept_to_create_addr = 10;
    control_msg.CMD = CTR_CMD_CREATE_EP;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_create_ep_param),
               sizeof(struct control_message_data_create_ept_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_CREATE_EP ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of create endpoint command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE != 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! creation of endpoints with similar address on different channels failed");

    // send message between endpoints in default channel and new channel.
    data_send_param.dest_addr = new_chnl->src;
    data_send_param.ept_to_ack_addr = app_chnl->src;
    env_memcpy((void *)data_send_param.msg, (void *)data, 5);
    data_send_param.msg_size = 5;
    control_msg.CMD = CTR_CMD_SEND;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_send_param), sizeof(struct control_message_data_send_param));
    result = rpmsg_sendto(app_chnl, (void *)(&control_msg), sizeof(struct control_message), new_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_SEND ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of send command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! send command operation on the other side failed");
    /* wait for responder's message */
    while (!message_received_2)
        ;
    env_lock_mutex(mutex);
    message_received_2 = 0;
    env_unlock_mutex(mutex);
    TEST_ASSERT_MESSAGE((0 == env_strncmp(trans_data, data, 4) ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! incorrect data received");

    // delete channel command
    /* delete channel not created yet */
    env_memcpy((void *)data_delete_channel_param.name, "channel not existed", 20);
    data_delete_channel_param.ep_to_ack_addr = app_chnl->src;
    control_msg.CMD = CTR_CMD_DESTROY_CHANNEL;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_delete_channel_param),
               sizeof(struct control_message_data_send_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_DESTROY_CHANNEL ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of delete channel command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE != 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! channel deletion of an unexisted channel on the other side failed");

    /* delete channel created before */
    env_memcpy((void *)data_delete_channel_param.name, (void *)name, sizeof(name));
    data_delete_channel_param.ep_to_ack_addr = app_chnl->src;
    control_msg.CMD = CTR_CMD_DESTROY_CHANNEL;
    control_msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)control_msg.DATA, (void *)(&data_delete_channel_param),
               sizeof(struct control_message_data_send_param));
    result = rpmsg_send(app_chnl, (void *)(&control_msg), sizeof(struct control_message));
    TEST_ASSERT_MESSAGE((0 == result ? 1 : (0 != ts_deinit_rpmsg())), "error! send message failed");
    /* wait for acknowledge message */
    while (!message_received)
        ;
    env_lock_mutex(mutex);
    message_received = 0;
    env_unlock_mutex(mutex);
    /* Check acknowledge message */
    TEST_ASSERT_MESSAGE((ack_msg.CMD_ACK == CTR_CMD_DESTROY_CHANNEL ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! expecting acknowledge of delete channel command operation");
    TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE == 0 ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! channel deletion operation on the other side failed");
    TEST_ASSERT_MESSAGE((new_chnl == NULL ? 1 : (0 != ts_deinit_rpmsg())),
                        "error! channel deletion operation on the this side failed");

    rdev->channel_created = test_channel_created;
    rdev->default_cb = test_read_cb;
    rdev->channel_destroyed = ts_channel_deleted;
    env_delete_mutex(mutex);
end:
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "system clean up error");
}

void run_tests(void *unused)
{
    RUN_TEST(tc_1_create_delete_ep_cmd_sender, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_2_send_cmd_sender, MAKE_UNITY_NUM(k_unity_rpmsg, 1));
    RUN_TEST(tc_3_create_delete_channel_cmd_sender, MAKE_UNITY_NUM(k_unity_rpmsg, 2));
}

/* EOF */
