#include "pingpong_common.h"
#include "unity.h"
#include "openamp/open_amp.h"
#include "openamp/rpmsg_rtos.h"
#include "assert.h"
#include "Freertos.h"
#include "task.h"
#include "string.h"

#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;

/*
 * utility: initialize rpmsg and enviroment
 * and wait for default channel
 */
int ts_init_rpmsg(void)
{
    env_init();
    env_sleep_msec(RPROC_BOOT_DELAY);
    int result = rpmsg_rtos_init(1, &rdev, RPMSG_REMOTE, &app_chnl);
    TEST_ASSERT_MESSAGE(0 == result, "error! init function failed");
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "error! init function failed");
    TEST_ASSERT_MESSAGE(NULL != rdev, "error! init function failed");
    return 0;
}

/*
 * utility: deinitialize rpmsg and enviroment
 */
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

/*
 * Destroy all created endpoints on the other side
 */
int ts_destroy_epts()
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    int ret_value;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ept_param;

    data_destroy_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_destroy_ept_param.ept_to_destroy_addr = DESTROY_ALL_EPT;

    msg.CMD = CTR_CMD_DESTROY_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_destroy_ept_param), sizeof(CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM));
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to send CTR_CMD_DESTROY_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE(CTR_CMD_DESTROY_EP == ack_msg.CMD_ACK,
                        "error! expecting acknowledge of CTR_CMD_DESTROY_EP copmmand");
    TEST_ASSERT_MESSAGE(0 == ack_msg.RETURN_VALUE, "error! failed to destroy endpoints on other side");

    return 0;
}

/*
 * Endpoint creation testing
 */
void tc_1_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *responder_ept;
    int ret_value, i = 0;
    int num_of_received_bytes = 0;
    unsigned long src;
    unsigned long ept_address;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;

    ret_value = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! init function failed");
    if (ret_value)
    {
        ret_value = ts_deinit_rpmsg();
        TEST_ASSERT_MESSAGE(0 == ret_value, "error! deinit function failed");
        return;
    }

    data_create_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;

    /*
     * Create endpoint with address = RPMSG_ADDR_ANY
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    data_create_ept_param.ept_to_create_addr = RPMSG_ADDR_ANY; /* Address to create endpoint */
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");

    /* Receive respond from other core */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on the other side");
    /* Get address of endpoint that was created with RPMSG_ADDR_ANY parameter*/
    env_memcpy((void *)&responder_ept, (void *)ack_msg.RESP_DATA, sizeof(unsigned long));
    ept_address = responder_ept->addr;

    /*
     * Create endpoint with address is address of endpoint that was created with RPMSG_ADDR_ANY parameter
     * This should not be possible. FALSE will returned.
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    /* Address to create endpoint */
    data_create_ept_param.ept_to_create_addr = ept_address;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");

    /* Receive respond from other core */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on the other side");

    /*
     * EP creation testing with another address
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    data_create_ept_param.ept_to_create_addr = 10; /* Address to create endpoint */
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on the other side");

    /*
     * Creation of the same EP should not be possible - ERROR should be returned
     */
    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on other side");

    /*
     * Create another endpoints (TC_EPT_COUNT - 3)
     */
    for (i = 0; i < TC_EPT_COUNT - 3; i++)
    {
        msg.CMD = CTR_CMD_CREATE_EP;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        data_create_ept_param.ept_to_create_addr++;
        env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
        /* Send command to create endpoint to the other core */
        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_CREATE_EP command to other side");
        /* Receive respond from other core */
        ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                    &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
        TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to create endpoint on other side");
    }

    /*
     * Testing destroy endpoints
     */
    ts_destroy_epts();
}

/*
 * TEST #2: Testing RPMSG pingpong1
 * rpmsg_rtos_recv() and rpmsg_rtos_send() functions are called on
 * the other side. Both blocking and non-blocking modes of the rpmsg_rtos_recv() function are tested.
 */
void tc_2_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *responder_ept, *sender_ept;
    unsigned long ept_address = INIT_EPT_ADDR;
    int ret_value, i = 0, testing_count = 0;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (unsigned int)4);
    data_send_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_COPY;

    data_recv_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = 0xffffffff;
    data_recv_param.mode = CMD_RECV_MODE_COPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 2; testing_count++)
    {
        for (i = 0; i < TEST_CNT; i++)
        {
            /*
             * Test receive function
             * Sender sends a request to create endpoint on the other side
             * Responder will receive the data from sender through this endpoint
             */
            msg.CMD = CTR_CMD_CREATE_EP;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param),
                       sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
            /* Send command to create endpoint to the other core */
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_CREATE_EP command to other side");
            /* Get respond from other side */
            ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept, (void *)ack_msg.RESP_DATA,
                       (unsigned int)(sizeof(struct rpmsg_endpoint *)));
            data_recv_param.responder_ept = responder_ept;

            /* send CTR_CMD_RECV command to the other sidr */
            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, "aaa", 3, data_recv_param.responder_ept->addr);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other core */
            ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_RECV copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            sender_ept = rpmsg_rtos_create_ept(app_chnl, ept_address);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_SEND command to other side");

            ret_value =
                rpmsg_rtos_recv(sender_ept, recv_buffer, &num_of_received_bytes, SENDER_APP_BUF_SIZE, &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_rtos_destroy_ept(sender_ept);

            /*
             * Destroy created endpoint on the other side
             */
            ts_destroy_epts();
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept = NULL;
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                    &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_RECV copmmand");
        TEST_ASSERT_MESSAGE((RPMSG_ERR_PARAM == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed when call rpmsg_rtos_recv function on the other side");

        /*
         * Test receive function on the other side with non-blocking call (timeout = 0)
         */
        data_recv_param.timeout_ms = 0;
    }
}

/*
 * TEST #3: Testing RPMSG pingpong2
 * rpmsg_rtos_recv_nocopy() and rpmsg_rtos_send() functions are called on
 * the other side. Both blocking and non-blocking modes of the rpmsg_rtos_recv_nocopy() function are tested.
 */
void tc_3_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *responder_ept, *sender_ept;
    unsigned long ept_address = INIT_EPT_ADDR;
    int ret_value, i = 0, testing_count = 0;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (unsigned int)4);
    data_send_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_COPY;

    data_recv_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = 0xffffffff;
    data_recv_param.mode = CMD_RECV_MODE_NOCOPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 2; testing_count++)
    {
        for (i = 0; i < TEST_CNT; i++)
        {
            /*
             * Test receive function
             * Sender sends a request to the responder to create endpoint on other side
             * Responder will receive the data from sender through this endpoint
             */
            msg.CMD = CTR_CMD_CREATE_EP;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param),
                       sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
            /* Send command to create endpoint to the other core */
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_CREATE_EP command to responder");
            ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from responder");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept, (void *)ack_msg.RESP_DATA,
                       (unsigned int)(sizeof(struct rpmsg_endpoint *)));
            data_recv_param.responder_ept = responder_ept;

            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, "aaa", 3, data_recv_param.responder_ept->addr);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other side */
            ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_RECV copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            sender_ept = rpmsg_rtos_create_ept(app_chnl, ept_address);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_SEND command to other side");

            ret_value =
                rpmsg_rtos_recv(sender_ept, recv_buffer, &num_of_received_bytes, SENDER_APP_BUF_SIZE, &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_rtos_destroy_ept(sender_ept);

            /*
             * Destroy created endpoint on the other side
             */
            ts_destroy_epts();
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept = NULL;
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                    &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_RECV copmmand");
        TEST_ASSERT_MESSAGE((RPMSG_ERR_PARAM == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed when call rpmsg_rtos_recv function on the other side");

        /*
         * Test receive function on the other side with non-blocking call (timeout = 0)
         */
        data_recv_param.timeout_ms = 0;
    }
}

/*
 * TEST #4: Testing timeout for receive function with copy mode
 */
void tc_4_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *responder_ept;
    unsigned long ept_address = INIT_EPT_ADDR;
    int ret_value;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;

    data_create_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    data_recv_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = CMD_RECV_TIMEOUT_MS;
    data_recv_param.mode = CMD_RECV_MODE_COPY;

    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on other side");
    env_memcpy((void *)&responder_ept, (void *)ack_msg.RESP_DATA, (unsigned int)(sizeof(struct rpmsg_endpoint *)));
    data_recv_param.responder_ept = responder_ept;

    /* Wait for a new message until the timeout expires, no message is sent. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    /* Get respond from other side */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV copmmand");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv function on the other side");
    TEST_ASSERT_MESSAGE(
        ((CMD_RECV_TIMEOUT_MS * 0.8 < ack_msg.TIMEOUT_MSEC < CMD_RECV_TIMEOUT_MS * 1.2) ? 1 : (0 != ts_destroy_epts())),
        "error! incorrect timeout received");

    /* Wait for a new message until the timeout expires, a message is sent at half of the timeout. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    env_sleep_msec(CMD_RECV_TIMEOUT_MS / 2);
    /* Send "aaa" string to other side */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, "aaa", 3, data_recv_param.responder_ept->addr);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send 'aaa' string to other side");
    /* Get respond from other side */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV copmmand");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv function on the other side");
    TEST_ASSERT_MESSAGE((((CMD_RECV_TIMEOUT_MS / 2) * 0.8 < ack_msg.TIMEOUT_MSEC < (CMD_RECV_TIMEOUT_MS / 2) * 1.2) ?
                             1 :
                             (0 != ts_destroy_epts())),
                        "error! incorrect timeout received");

    /*
     * Destroy created endpoint on the other side
     */
    ts_destroy_epts();
}

/*
 * Testing timeout for receive function with no-copy mode
 */
void tc_5_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *responder_ept;
    unsigned long ept_address = INIT_EPT_ADDR;
    int ret_value;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;

    data_create_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    data_recv_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = CMD_RECV_TIMEOUT_MS;
    data_recv_param.mode = CMD_RECV_MODE_NOCOPY;

    msg.CMD = CTR_CMD_CREATE_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param), sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
    /* Send command to create endpoint to the other core */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_CREATE_EP command to other side");
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to create endpoint on other side");
    env_memcpy((void *)&responder_ept, (void *)ack_msg.RESP_DATA, (unsigned int)(sizeof(struct rpmsg_endpoint *)));
    data_recv_param.responder_ept = responder_ept;

    /* Wait for a new message until the timeout expires, no message is sent. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    /* Get respond from other side */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV copmmand");
    TEST_ASSERT_MESSAGE((0 != ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv function on the other side");
    TEST_ASSERT_MESSAGE(
        ((CMD_RECV_TIMEOUT_MS * 0.8 < ack_msg.TIMEOUT_MSEC < CMD_RECV_TIMEOUT_MS * 1.2) ? 1 : (0 != ts_destroy_epts())),
        "error! incorrect timeout received");

    /* Wait for a new message until the timeout expires, a message is sent at half of the timeout. */
    msg.CMD = CTR_CMD_RECV;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send CTR_CMD_RECV command to other side");
    env_sleep_msec(CMD_RECV_TIMEOUT_MS / 2);
    /* Send "aaa" string to other side */
    ret_value = rpmsg_rtos_send(app_chnl->rp_ept, "aaa", 3, data_recv_param.responder_ept->addr);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to send 'aaa' string to other side");
    /* Get respond from other side */
    ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                        "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                        "error! expecting acknowledge of CTR_CMD_RECV copmmand");
    TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                        "error! failed when call rpmsg_rtos_recv on the other side");
    TEST_ASSERT_MESSAGE((((CMD_RECV_TIMEOUT_MS / 2) * 0.8 < ack_msg.TIMEOUT_MSEC < (CMD_RECV_TIMEOUT_MS / 2) * 1.2) ?
                             1 :
                             (0 != ts_destroy_epts())),
                        "error! incorrect timeout received");

    /*
     * Destroy created endpoint on the other side
     */
    ts_destroy_epts();
}

/*
 * TEST #6: Testing RPMSG pingpong3
 * rpmsg_rtos_recv() and rpmsg_rtos_send_nocopy() functions are called on
 * the other side. Both blocking and non-blocking modes of the rpmsg_rtos_recv() function are tested.
 */
void tc_6_main_task(void)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *responder_ept, *sender_ept;
    unsigned long ept_address = INIT_EPT_ADDR;
    int ret_value, i = 0, testing_count = 0;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (unsigned int)4);
    data_send_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_NOCOPY;

    data_recv_param.ept_to_ack_addr = app_chnl->rp_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = 0xffffffff;
    data_recv_param.mode = CMD_RECV_MODE_NOCOPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 2; testing_count++)
    {
        for (i = 0; i < TEST_CNT; i++)
        {
            /*
             * Test receive function
             * Sender sends a request to the responder to create endpoint on other side
             * Responder will receive the data from sender through this endpoint
             */
            msg.CMD = CTR_CMD_CREATE_EP;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)(&data_create_ept_param),
                       sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));
            /* Send command to create endpoint to the other core */
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_CREATE_EP command to responder");
            ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from responder");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept, (void *)ack_msg.RESP_DATA,
                       (unsigned int)(sizeof(struct rpmsg_endpoint *)));
            data_recv_param.responder_ept = responder_ept;

            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, "aaa", 3, data_recv_param.responder_ept->addr);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other side */
            ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                                "error! expecting acknowledge of CTR_CMD_RECV copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            ept_address = INIT_EPT_ADDR + 1;
            sender_ept = rpmsg_rtos_create_ept(app_chnl, ept_address);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to send CTR_CMD_SEND command to other side");

            ret_value =
                rpmsg_rtos_recv(sender_ept, recv_buffer, &num_of_received_bytes, SENDER_APP_BUF_SIZE, &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : (0 != ts_destroy_epts())),
                                "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_rtos_destroy_ept(sender_ept);

            /*
             * Destroy created endpoint on the other side
             */
            ts_destroy_epts();
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept = NULL;
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), app_chnl->dst);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                    &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : (0 != ts_destroy_epts())),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : (0 != ts_destroy_epts())),
                            "error! expecting acknowledge of CTR_CMD_RECV copmmand");
        TEST_ASSERT_MESSAGE((RPMSG_ERR_PARAM == ack_msg.RETURN_VALUE ? 1 : (0 != ts_destroy_epts())),
                            "error! failed when call rpmsg_rtos_recv function on the other side");

        /*
         * Test receive function on the other side with non-blocking call (timeout = 0)
         */
        data_recv_param.timeout_ms = 0;
    }
}

void run_tests(void *unused)
{
    RUN_TEST(tc_1_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_2_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_3_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_4_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_5_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
    RUN_TEST(tc_6_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
