#include "pingpong_common.h"
#include "unity.h"
#include "openamp/open_amp.h"
#include "openamp/rpmsg_rtos.h"
#include "assert.h"
#include "Freertos.h"
#include "task.h"

#define TC_TRANSFER_COUNT 10
#define DATA_LEN 45

struct remote_device *rdev = NULL;
struct rpmsg_channel *app_chnl = NULL;
struct rpmsg_endpoint *endpoints[TC_EPT_COUNT] = {NULL};
int ept_num = 0;

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

/******************************************************************************
 * Responder task
 *****************************************************************************/
void responder_task(void)
{
    int ret_value = 0;
    void *data_addr = NULL;
    int num_of_received_control_bytes;
    unsigned long i;
    unsigned long src = 0;
    struct rpmsg_endpoint *ept;
    CONTROL_MESSAGE msg;
    ACKNOWLEDGE_MESSAGE ack_msg;
    CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM data_create_ept_param;
    CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM data_destroy_ept_param;
    CONTROL_MESSAGE_DATA_RECV_PARAM data_recv_param;
    CONTROL_MESSAGE_DATA_SEND_PARAM data_send_param;
    unsigned char *recv_buffer[BUFFER_MAX_LENGTH];
    void *nocopy_buffer_ptr = NULL; // pointer to receive data in no-copy mode
    unsigned long buf_size = 0;     /* use to store size of buffer for
                                       rpmsg_rtos_alloc_tx_buffer() */

    ret_value = ts_init_rpmsg();
    TEST_ASSERT_MESSAGE(0 == ret_value, "Testing function init rpmsg");
    if (ret_value)
        goto end;

    while (1)
    {
        ret_value = rpmsg_rtos_recv(app_chnl->rp_ept, &msg, &num_of_received_control_bytes, sizeof(CONTROL_MESSAGE),
                                    &src, 0xFFFFFFFF);

        if (0 != ret_value)
        {
           // printf("Responder task receive error: %i\n", ret_value);
        }
        else
        {
           // printf("Responder task received a msg\n\r");
           // printf("Message: Size=0x%x, CMD = 0x%x, DATA = 0x%x 0x%x 0x%x 0x%x\n\r", num_of_received_control_bytes,
           //        msg.CMD, msg.DATA[0], msg.DATA[1], msg.DATA[2], msg.DATA[3]);

            switch (msg.CMD)
            {
                case CTR_CMD_CREATE_EP:
                    ret_value = -1;
                    env_memcpy((void *)(&data_create_ept_param), (void *)msg.DATA,
                               sizeof(CONTROL_MESSAGE_DATA_CREATE_EPT_PARAM));

                    ept = rpmsg_rtos_create_ept(app_chnl, data_create_ept_param.ept_to_create_addr);
                    if (NULL == ept)
                    {
                        ret_value = 1; /* Fail to create enpoint */
                    }
                    else
                    {
                        ret_value = 2;
                        for(i=0;i<TC_EPT_COUNT;i++)
                        {
                          if(endpoints[i]==NULL)
                          {
                            endpoints[i] = ept;
                            ept_num++;
                            ret_value = 0;
                            break;
                          }
                        }
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_CREATE_EP;
                        ack_msg.RETURN_VALUE = ret_value;
                        env_memcpy((void *)ack_msg.RESP_DATA, (void *)&(ept->addr), sizeof(unsigned long));
                        /* Send ack_msg to sender */
                        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                                    data_create_ept_param.ept_to_ack_addr);
                    }
                    break;
                case CTR_CMD_DESTROY_EP:
                    ret_value = -1;
                    env_memcpy((void *)(&data_destroy_ept_param), (void *)msg.DATA,
                               sizeof(CONTROL_MESSAGE_DATA_DESTROY_EPT_PARAM));

                    if (data_destroy_ept_param.ept_to_destroy_addr == DESTROY_ALL_EPT)
                    {
                        for (i = 0; i < TC_EPT_COUNT; i++)
                        {
                          if(endpoints[i]!= NULL)
                          {
                            rpmsg_rtos_destroy_ept(endpoints[i]);
                            ept_num--;
                            endpoints[i] = NULL;
                            /* We can't check return value of destroy function due to this function return void */
                            ret_value = 0;
                          }
                        }
                    }
                    else
                    {
                        ret_value = 1;
                        for (i = 0; i < TC_EPT_COUNT; i++)
                        {
                          if(endpoints[i]!= NULL)
                          {
                            if(data_destroy_ept_param.ept_to_destroy_addr == endpoints[i]->addr)
                            {

                              rpmsg_rtos_destroy_ept(endpoints[i]);
                              ept_num--;
                              endpoints[i] = NULL;
                              /* We can't check return value of destroy function due to this function return void */
                              ret_value = 0;
                              break;
                            }
                          }
                        }

                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_DESTROY_EP;
                        ack_msg.RETURN_VALUE = ret_value;
                        /* Send ack_msg to tea_control_endpoint */
                        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                                    data_destroy_ept_param.ept_to_ack_addr);
                    }
                    break;
                case CTR_CMD_RECV:
                    ret_value = -1;
                    env_memcpy((void *)&data_recv_param, (void *)msg.DATA,
                               (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_RECV_PARAM)));

                    ept = NULL;
				    for (i = 0; i < TC_EPT_COUNT; i++)
				    {
				      if(endpoints[i]!= NULL)
				      {
					    if(data_recv_param.responder_ept_addr == endpoints[i]->addr)
					    {
						    ept = endpoints[i];
						    // printf("Found ept with address %d\n", data_recv_param.responder_ept_addr);
					    }
				      }
				    }

                    if(ept == NULL)
                    {
                        ret_value = -1;
                    }
                    else if (CMD_RECV_MODE_COPY == data_recv_param.mode)
                    {
                        if (0xFFFFFFFF == data_recv_param.timeout_ms)
                        {
                            ret_value = rpmsg_rtos_recv(ept, recv_buffer,
                                                        &num_of_received_control_bytes, data_recv_param.buffer_size,
                                                        &src, data_recv_param.timeout_ms);
                        }
                        else if (0 == data_recv_param.timeout_ms)
                        {
                            /* receive function with non-blocking call */
                            do
                            {
                                ret_value = rpmsg_rtos_recv(ept, recv_buffer,
                                                            &num_of_received_control_bytes, data_recv_param.buffer_size,
                                                            &src, data_recv_param.timeout_ms);

                                if (ret_value == RPMSG_ERR_PARAM)
                                    break;
                            } while (0 != ret_value);
                        }
                        else
                        {
                            TickType_t tick_count = xTaskGetTickCount();
                            ret_value = rpmsg_rtos_recv(ept, recv_buffer,
                                                        &num_of_received_control_bytes, data_recv_param.buffer_size,
                                                        &src, data_recv_param.timeout_ms);
                            tick_count = xTaskGetTickCount() - tick_count;
                            // Calculate milisecond
                            ack_msg.TIMEOUT_MSEC = tick_count * (1000 / configTICK_RATE_HZ);
                        }
                    }
                    else if (CMD_RECV_MODE_NOCOPY == data_recv_param.mode)
                    {
                        if (0xFFFFFFFF == data_recv_param.timeout_ms)
                        {
                            ret_value = rpmsg_rtos_recv_nocopy(ept, &nocopy_buffer_ptr,
                                                               &num_of_received_control_bytes, &src,
                                                               data_recv_param.timeout_ms);
                        }
                        else if (0 == data_recv_param.timeout_ms)
                        {
                            /* receive function with non-blocking call */
                            do
                            {
                                ret_value = rpmsg_rtos_recv_nocopy(ept, &nocopy_buffer_ptr,
                                                                   &num_of_received_control_bytes, &src,
                                                                   data_recv_param.timeout_ms);

                                if (ret_value == RPMSG_ERR_PARAM)
                                    break;
                            } while (0 != ret_value);
                        }
                        else
                        {
                            TickType_t tick_count = xTaskGetTickCount();
                            ret_value = rpmsg_rtos_recv_nocopy(ept, &nocopy_buffer_ptr,
                                                               &num_of_received_control_bytes, &src,
                                                               data_recv_param.timeout_ms);
                            tick_count = xTaskGetTickCount() - tick_count;
                            // Calculate milisecond
                            ack_msg.TIMEOUT_MSEC = tick_count * (1000 / configTICK_RATE_HZ);
                        }

                        /* Free buffer when use rpmsg_rtos_recv_nocopy function */
                        if (0 == ret_value)
                        {
                            env_memcpy((void *)recv_buffer, (void *)nocopy_buffer_ptr, num_of_received_control_bytes);
                            ret_value = rpmsg_rtos_release_rx_buffer(ept, nocopy_buffer_ptr);
                        }
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_RECV;
                        ack_msg.RETURN_VALUE = ret_value;

                        env_memcpy((void *)ack_msg.RESP_DATA, (void *)recv_buffer, num_of_received_control_bytes);
                        env_memset(recv_buffer, 0, BUFFER_MAX_LENGTH);
                        /* Send ack_msg to sender */
                        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                                    data_recv_param.ept_to_ack_addr);
                    }

                    break;
                case CTR_CMD_SEND:
                    ret_value = -1;
                    env_memcpy((void *)&data_send_param, (void *)msg.DATA,
                               (unsigned int)(sizeof(CONTROL_MESSAGE_DATA_SEND_PARAM)));

                    if (CMD_SEND_MODE_COPY == data_send_param.mode)
                    {
                        for (i = 0; i < data_send_param.repeat_count; i++)
                        {
                            ret_value = rpmsg_rtos_send(app_chnl->rp_ept, data_send_param.msg, data_send_param.msg_size,
                                                        data_send_param.dest_addr);
                        }
                    }
                    else if (CMD_SEND_MODE_NOCOPY == data_send_param.mode)
                    {
                        for (i = 0; i < data_send_param.repeat_count; i++)
                        {
                            data_addr = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, &buf_size);
                            if (buf_size == 0 || data_addr == NULL)
                            {
                                ret_value = -1; /* Failed to alloc tx buffer */
                                break;
                            }
                            env_memcpy((void *)data_addr, (void *)data_send_param.msg, data_send_param.msg_size);
                            ret_value = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, data_addr, data_send_param.msg_size,
                                                               data_send_param.dest_addr);
                        }
                    }

                    if (ACK_REQUIRED_YES == msg.ACK_REQUIRED)
                    {
                        ack_msg.CMD_ACK = CTR_CMD_SEND;
                        ack_msg.RETURN_VALUE = ret_value;
                        /* Send ack_msg to tea_control_endpoint */
                        ret_value = rpmsg_rtos_send(app_chnl->rp_ept, &ack_msg, sizeof(ACKNOWLEDGE_MESSAGE),
                                                    data_send_param.ept_to_ack_addr);
                    }

                    break;
            }
        }
    }

end:
    ret_value = ts_deinit_rpmsg();
    TEST_ASSERT_MESSAGE(0 == ret_value, "negative number");
}

void run_tests(void *unused)
{
    RUN_TEST(responder_task, MAKE_UNITY_NUM(k_unity_rpmsg, 0));
}
