#include "pingpong_common.h"
#include "unity.h"
#include "openamp/open_amp.h"
#include "openamp/rpmsg_rtos.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "stdint.h"

#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45
#define WORKER_NUMBER (3) /* Number of concurrent threads, needs enough heap... */
#define TEST_EPT_NUM_BASE (60)
#define REMOTE_DEFAULT_EPT (1024)

typedef struct
{
  struct rpmsg_endpoint * ack_ept;
  uint8_t worker_id;

}initParamTypedef;

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;
volatile int globCntr = 0;

void sendRecvTestTask(void * ept_number);

/*
 * utility: initialize rpmsg and enviroment
 * and wait for default channel
 */
int ts_init_rpmsg(void)
{
    env_init();
    int result = rpmsg_rtos_init(0, &rdev, RPMSG_MASTER, &app_chnl);
    TEST_ASSERT_MESSAGE(0 == result, "Testing return value of rpmsg_rtos_init");
    TEST_ASSERT_MESSAGE(NULL != app_chnl, "app_chnl is not NULL");
    TEST_ASSERT_MESSAGE(NULL != rdev, "rdev is not NULL");
    return result;
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
 * Destroy an endpoint on the other side
 */
int ts_destroy_ept(int addr, struct rpmsg_endpoint * ack_ept)
{
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    int ret_value;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ept_param;

    data_destroy_ept_param.ept_to_ack_addr = ack_ept->addr;
    data_destroy_ept_param.ept_to_destroy_addr = addr;

    msg.CMD = CTR_CMD_DESTROY_EP;
    msg.ACK_REQUIRED = ACK_REQUIRED_YES;
    env_memcpy((void *)msg.DATA, (void *)(&data_destroy_ept_param), sizeof(CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM));
    ret_value = rpmsg_rtos_send(ack_ept, &msg, sizeof(CONTROL_MESSAGE), REMOTE_DEFAULT_EPT);
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to send CTR_CMD_DESTROY_EP command to other side");
    /* Receive respond from other core */
    ret_value = rpmsg_rtos_recv(ack_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE), &src,
                                0xFFFFFFFF);
    TEST_ASSERT_MESSAGE(0 == ret_value, "error! failed to receive acknowledge message from other side");
    TEST_ASSERT_MESSAGE(CTR_CMD_DESTROY_EP == ack_msg.CMD_ACK,
                        "error! expecting acknowledge of CTR_CMD_DESTROY_EP copmmand");
    TEST_ASSERT_MESSAGE(0 == ack_msg.RETURN_VALUE, "error! failed to destroy endpoints on other side");

    return 0;
}

/*
 * Thread safety testing
 */
void tc_1_main_task(void)
{
    int result, i;
    initParamTypedef* initParam = NULL;
    long unsigned int src;
    int recved;
    char buf[32];
    CONTROL_MESSAGE msg = {0};

    result = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
    if (result == 0)
    {
        rpmsg_rtos_recv(app_chnl->rp_ept, buf, &recved, 32, &src, 0xFFFFFFFF);
        for(i = 0; i < WORKER_NUMBER; i++)
        {
            initParam = pvPortMalloc(sizeof(initParamTypedef));
            assert(initParam);
            initParam->worker_id = (uint8_t)i;
            initParam->ack_ept = rpmsg_rtos_create_ept(app_chnl, RPMSG_ADDR_ANY);
            assert(initParam->ack_ept);
            result = xTaskCreate(sendRecvTestTask, "THREAD_SAFETY_TASK", 384, (void*)(initParam), tskIDLE_PRIORITY + 2, NULL);
            assert(pdPASS == result);
        }

        globCntr = 0;
        while(globCntr < WORKER_NUMBER)
        {
          vTaskDelay(100);
        }

        /* Send command to end to the other core */
        msg.CMD = CTR_CMD_DESTROY_CHANNEL;
        msg.ACK_REQUIRED = ACK_REQUIRED_NO;
        rpmsg_rtos_send(app_chnl->rp_ept, &msg, sizeof(CONTROL_MESSAGE), REMOTE_DEFAULT_EPT);

    }
    result = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == result, "negative number");
}

void sendRecvTestTask(void * initParam)
{
    initParamTypedef* init = (initParamTypedef*)initParam;
    assert(init);
    CONTROL_MESSAGE msg = {0};
    ACKNOWLEDGE_MESSAGE ack_msg = {0};
    struct rpmsg_endpoint *sender_ept;
    unsigned long responder_ept_addr = -1;
    unsigned long ept_address =  TEST_EPT_NUM_BASE + init->worker_id;
    char worker_id_str[2] = {init->worker_id + 'a', '\0'};
    int ret_value, i = 0, testing_count = 0;
    int num_of_received_bytes = 0;
    unsigned long src;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    char recv_buffer[SENDER_APP_BUF_SIZE];

    data_create_ept_param.ept_to_ack_addr = init->ack_ept->addr;
    data_create_ept_param.ept_to_create_addr = ept_address;

    env_memcpy((void *)data_send_param.msg, "abc", (unsigned int)4);
    data_send_param.ept_to_ack_addr = init->ack_ept->addr;
    data_send_param.msg_size = CMD_SEND_MSG_SIZE;
    data_send_param.repeat_count = 1;
    data_send_param.mode = CMD_SEND_MODE_COPY;

    data_recv_param.ept_to_ack_addr = init->ack_ept->addr;
    data_recv_param.buffer_size = RESPONDER_APP_BUF_SIZE;
    data_recv_param.timeout_ms = 0xffffffff;
    data_recv_param.mode = CMD_RECV_MODE_COPY;

    /* Testing with blocking call and non-blocking call (timeout = 0) */
    for (testing_count = 0; testing_count < 16; testing_count++)
    {
        UnityPrint(worker_id_str); /* Print a dot, showing progress */

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
            ret_value = rpmsg_rtos_send(init->ack_ept, &msg, sizeof(CONTROL_MESSAGE), REMOTE_DEFAULT_EPT);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to send CTR_CMD_CREATE_EP command to other side");
            /* Get respond from other side */
            ret_value = rpmsg_rtos_recv(init->ack_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_CREATE_EP == ack_msg.CMD_ACK ? 1 : 0),
                                "error! expecting acknowledge of CTR_CMD_CREATE_EP copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : 0),
                                "error! failed to create endpoint on other side");
            env_memcpy((void *)&responder_ept_addr, (void *)ack_msg.RESP_DATA,
                       (unsigned int)(sizeof(unsigned long)));
            data_recv_param.responder_ept_addr = responder_ept_addr;

            /* send CTR_CMD_RECV command to the other side */
            msg.CMD = CTR_CMD_RECV;
            msg.ACK_REQUIRED = ACK_REQUIRED_YES;
            env_memcpy((void *)msg.DATA, (void *)&data_recv_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
            ret_value = rpmsg_rtos_send(init->ack_ept, &msg, sizeof(CONTROL_MESSAGE), REMOTE_DEFAULT_EPT);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to send CTR_CMD_RECV command to other side");

            /* Send "aaa" string to other side */
            ret_value = rpmsg_rtos_send(init->ack_ept, "aaa", 3, data_recv_param.responder_ept_addr);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to send 'aaa' string to other side");

            /* Get respond from other core */
            ret_value = rpmsg_rtos_recv(init->ack_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                        &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to receive acknowledge message from other side");
            TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : 0),
                                "error! expecting acknowledge of CTR_CMD_RECV copmmand");
            TEST_ASSERT_MESSAGE((0 == ack_msg.RETURN_VALUE ? 1 : 0),
                                "error! failed when call rpmsg_rtos_recv function on the other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(ack_msg.RESP_DATA, "aaa", 3) ? 1 : 0),
                                "error! incorrect data received");

            /*
             * Test send function
             * Create a new endpoint on the sender side and sender will receive data through this endpoint
             */
            sender_ept = rpmsg_rtos_create_ept(app_chnl, ept_address);
            TEST_ASSERT_MESSAGE((NULL != sender_ept ? 1 : 0),
                                "error! failed to create endpoint");

            data_send_param.dest_addr = sender_ept->addr;

            msg.CMD = CTR_CMD_SEND;
            msg.ACK_REQUIRED = ACK_REQUIRED_NO;
            env_memcpy((void *)msg.DATA, (void *)&data_send_param,
                       (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));
            ret_value = rpmsg_rtos_send(init->ack_ept, &msg, sizeof(CONTROL_MESSAGE), REMOTE_DEFAULT_EPT);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to send CTR_CMD_SEND command to other side");

            ret_value =
                rpmsg_rtos_recv(sender_ept, recv_buffer, &num_of_received_bytes, SENDER_APP_BUF_SIZE, &src, 0xFFFFFFFF);
            TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                                "error! failed to receive data from other side");
            TEST_ASSERT_MESSAGE((0 == strncmp(recv_buffer, "abc", 3) ? 1 : 0),
                                "error! incorrect data received");

            /*
             * Destroy created endpoint on the sender side
             */
            rpmsg_rtos_destroy_ept(sender_ept);

            ts_destroy_ept(responder_ept_addr, init->ack_ept);
        }

        /*
         * Attempt to call receive function on the other side with the invalid EP pointer (not yet created EP)
         */
        data_recv_param.responder_ept_addr = -1;
        msg.CMD = CTR_CMD_RECV;
        msg.ACK_REQUIRED = ACK_REQUIRED_YES;
        env_memcpy((void *)msg.DATA, (void *)&data_recv_param, (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));
        ret_value = rpmsg_rtos_send(init->ack_ept, &msg, sizeof(CONTROL_MESSAGE), REMOTE_DEFAULT_EPT);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                            "error! failed to send CTR_CMD_RECV command to other side");
        /* Get respond from other side */
        ret_value = rpmsg_rtos_recv(init->ack_ept, &ack_msg, &num_of_received_bytes, sizeof(ACKNOWLEDGE_MESSAGE),
                                    &src, 0xFFFFFFFF);
        TEST_ASSERT_MESSAGE((0 == ret_value ? 1 : 0),
                            "error! failed to receive acknowledge message from other side");
        TEST_ASSERT_MESSAGE((CTR_CMD_RECV == ack_msg.CMD_ACK ? 1 : 0),
                            "error! expecting acknowledge of CTR_CMD_RECV copmmand");
        TEST_ASSERT_MESSAGE((ack_msg.RETURN_VALUE < 0 ? 1 : 0),
                            "error! failed when call rpmsg_rtos_recv function on the other side");
    }
    globCntr++;

    rpmsg_rtos_destroy_ept(init->ack_ept);
    vPortFree(init);
    vTaskDelete(NULL);
}


void run_tests(void *unused)
{
    RUN_TEST(tc_1_main_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
