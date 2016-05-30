/*
 * Copyright (c) 2014, Mentor Graphics Corporation
 * All rights reserved.
 * Copyright (c) 2015 Xilinx, Inc. All rights reserved.
 * Copyright 2016 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of Mentor Graphics Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "openamp/rpmsg_lite.h"

/* Structure describing the local instance
 * of RPMSG lite communication stack and
 * holds all runtime variables needed internally
 * by the stack.
 */
struct rpmsg_lite_instance
{
    struct virtio_device virt_dev;
    struct virtqueue *rvq;
    struct virtqueue *tvq;
    unsigned int remote_role;
    struct llist *rl_endpoints;
    LOCK *lock;
    unsigned int link_state;
    char *sh_mem_base;
    unsigned int sh_mem_remaining; 
    unsigned int sh_mem_total;
    struct virtqueue_ops const *vq_ops;
};

/* rpmsg_std_hdr contains a reserved field,
 * this implementation of RPMSG uses this reserved
 * field to hold the idx and totlen of the buffer
 * not being returned to the vring in the receive
 * callback function. This way, the no-copy API
 * can use this field to return the buffer later.
 */
struct rpmsg_hdr_reserved
{
    short int idx;
    short int totlen;
};

/* Interface which is used to interact with the virtqueue layer,
 * a different interface is used, when the local processor is the MASTER
 * and when it is the REMOTE.
 */
struct virtqueue_ops
{
    void (*vq_tx)(void *buffer, unsigned long len, unsigned short idx);
    void* (*vq_tx_alloc)(unsigned long *len, unsigned short *idx);
    void* (*vq_rx)(unsigned long *len, unsigned short *idx);
    void (*vq_rx_free)(void *buffer, unsigned long len, unsigned short idx);
};

/* Interface to the low level */
extern struct hil_proc *platform_get_processor_by_id(int cpu_id);

/* Instance of the local rpmsg lite device, avoiding usage of heap */
static volatile struct rpmsg_lite_instance rpmsg_lite_dev = {0};

/* Zero-Copy extension macros */
#define RPMSG_STD_HDR_FROM_BUF(buf)   (struct rpmsg_std_hdr *)((char*)buf - \
                                       offsetof(struct rpmsg_std_hdr, data))


/**
 * rpmsg_lite_get_endpoint_from_addr
 *
 * Create a new rpmsg endpoint, which can be used
 * for communication.
 *
 * @param addr   - local endpoint address
 *
 * @return       - RL_NULL if not found,
 *			       node pointer containing the ept on success
 *
 */
struct llist *
rpmsg_lite_get_endpoint_from_addr(unsigned long addr)
{
    struct llist *rl_ept_lut_head;

    rl_ept_lut_head = rpmsg_lite_dev.rl_endpoints;
    while (rl_ept_lut_head) 
    {
        struct rpmsg_lite_endpoint *rl_ept = 
          (struct rpmsg_lite_endpoint *) rl_ept_lut_head->data;
        if (rl_ept->addr == addr) 
        {
            return rl_ept_lut_head;
        }
        rl_ept_lut_head = rl_ept_lut_head->next;
    }
    return RL_NULL ;
}

/***************************************************************
   mmm    mm   m      m      mmmmm    mm     mmm  m    m  mmmm 
 m"   "   ##   #      #      #    #   ##   m"   " #  m"  #"   "
 #       #  #  #      #      #mmmm"  #  #  #      #m#    "#mmm 
 #       #mm#  #      #      #    #  #mm#  #      #  #m      "#
  "mmm" #    # #mmmmm #mmmmm #mmmm" #    #  "mmm" #   "m "mmm#"
****************************************************************/

/**
 * rpmsg_lite_rx_callback
 *
 * Called when remote side calls virtqueue_kick() 
 * at its transmit virtqueue. 
 * In this callback, the buffer is read-out
 * of the rvq and user callback is called.
 *
 * @param vq - virtqueue affected by the kick
 *
 */
static void rpmsg_lite_rx_callback(struct virtqueue *vq) 
{
    struct rpmsg_std_hdr *rpmsg_hdr;
    unsigned long len;
    unsigned short idx;
    struct rpmsg_lite_endpoint *ept;
    int cb_ret;
    struct llist *node;


    /* Process the received data from remote node */
    rpmsg_hdr = (struct rpmsg_std_hdr *) rpmsg_lite_dev.vq_ops->vq_rx(&len, &idx);

     while(rpmsg_hdr) 
     {
       
        node = rpmsg_lite_get_endpoint_from_addr(rpmsg_hdr->dst);
       
        cb_ret = RL_RELEASE;
        if(node != RL_NULL)
        {
          ept = (struct rpmsg_lite_endpoint *)node->data;
          cb_ret = ept->rx_cb(rpmsg_hdr->data, rpmsg_hdr->len,
                          rpmsg_hdr->src, ept->rx_cb_data);
        }

        if(cb_ret == RL_HOLD)
        {
            ((struct rpmsg_hdr_reserved*)&rpmsg_hdr->reserved)->idx = idx;
            ((struct rpmsg_hdr_reserved*)&rpmsg_hdr->reserved)->totlen = len;
        }
        else
        {
            rpmsg_lite_dev.vq_ops->vq_rx_free(rpmsg_hdr, len, idx);
        }
        rpmsg_hdr = (struct rpmsg_std_hdr *) rpmsg_lite_dev.vq_ops->vq_rx(&len, &idx);
     }
}

/**
 * rpmsg_lite_tx_callback
 *
 * Called when remote side calls virtqueue_kick() 
 * at its receive virtqueue. 
 *
 * @param vq - virtqueue affected by the kick
 *
 */
static void rpmsg_lite_tx_callback(struct virtqueue *vq) 
{
  rpmsg_lite_dev.link_state = 1;
}


/****************************************************************************

 m    m  mmmm         m    m   mm   mm   m mmmm   m      mmmmm  mm   m   mmm 
 "m  m" m"  "m        #    #   ##   #"m  # #   "m #        #    #"m  # m"   "
  #  #  #    #        #mmmm#  #  #  # #m # #    # #        #    # #m # #   mm
  "mm"  #    #        #    #  #mm#  #  # # #    # #        #    #  # # #    #
   ##    #mm#"        #    # #    # #   ## #mmm"  #mmmmm mm#mm  #   ##  "mmm"
            #
 In case this processor has the REMOTE role
*****************************************************************************/

/**
 * vq_tx_remote
 *
 * Places buffer on the virtqueue for consumption by the other side.
 *
 * @param buffer - buffer pointer
 * @param len    - buffer length
 * @idx          - buffer index
 *
 * @return - status of function execution
 *
 */
static void vq_tx_remote(void *buffer, unsigned long len, unsigned short idx) 
{
    int status;
    status = virtqueue_add_consumed_buffer(rpmsg_lite_dev.tvq, idx, len);
    RL_ASSERT(status == VQUEUE_SUCCESS); /* must success here */
    
    /* As long as the length of the virtqueue ring buffer is not shorter
     * than the number of buffers in the pool, this function should not fail.
     * This condition is always met, so we don't need to return anything here */
}

/**
 * vq_tx_alloc_remote
 *
 * Provides buffer to transmit messages.
 *
 * @param len  - length of returned buffer
 * @param idx  - buffer index
 *
 * return - pointer to buffer.
 */
static void *vq_tx_alloc_remote(unsigned long *len, unsigned short *idx)
{

    return virtqueue_get_available_buffer(rpmsg_lite_dev.tvq, idx,
                        (uint32_t *) len);
}

/**
 * vq_rx_remote
 *
 * Retrieves the received buffer from the virtqueue.
 *
 * @param len  - size of received buffer
 * @param idx  - index of buffer
 *
 * @return - pointer to received buffer
 *
 */
static void *vq_rx_remote(unsigned long *len, unsigned short *idx)
{
    return virtqueue_get_available_buffer(rpmsg_lite_dev.rvq, idx, (uint32_t*)len);
}

/**
 * vq_rx_free_remote
 *
 * Places the used buffer back on the virtqueue.
 *
 * @param buffer - buffer pointer
 * @param len    - buffer length
 * @param idx    - buffer index
 *
 */
static void vq_rx_free_remote(void *buffer, unsigned long len, unsigned short idx)
{
    int status;
 
    status = virtqueue_add_consumed_buffer(rpmsg_lite_dev.rvq, idx, len);
    RL_ASSERT(status == VQUEUE_SUCCESS); /* must success here */
    /* As long as the length of the virtqueue ring buffer is not shorter
     * than the number of buffers in the pool, this function should not fail.
     * This condition is always met, so we don't need to return anything here */
}

/****************************************************************************

 m    m  mmmm         m    m   mm   mm   m mmmm   m      mmmmm  mm   m   mmm 
 "m  m" m"  "m        #    #   ##   #"m  # #   "m #        #    #"m  # m"   "
  #  #  #    #        #mmmm#  #  #  # #m # #    # #        #    # #m # #   mm
  "mm"  #    #        #    #  #mm#  #  # # #    # #        #    #  # # #    #
   ##    #mm#"        #    # #    # #   ## #mmm"  #mmmmm mm#mm  #   ##  "mmm"
            #
 In case this processor has the MASTER role
*****************************************************************************/

/**
 * vq_tx_master
 *
 * Places buffer on the virtqueue for consumption by the other side.
 *
 * @param buffer - buffer pointer
 * @param len    - buffer length
 * @idx          - buffer index
 *
 * @return - status of function execution
 *
 */
static void vq_tx_master(void *buffer, unsigned long len, unsigned short idx)
{
    struct llist node;
    int status;
    /* Initialize buffer node */
    node.data = buffer;
    node.attr = len;
    node.next = RL_NULL;
    node.prev = RL_NULL;
    status = virtqueue_add_buffer(rpmsg_lite_dev.tvq, &node, 0, 1, buffer);
    RL_ASSERT(status == VQUEUE_SUCCESS); /* must success here */
    
    /* As long as the length of the virtqueue ring buffer is not shorter
     * than the number of buffers in the pool, this function should not fail.
     * This condition is always met, so we don't need to return anything here */
}


/**
 * vq_tx_alloc_master
 *
 * Provides buffer to transmit messages.
 *
 * @param len  - length of returned buffer
 * @param idx  - buffer index
 *
 * return - pointer to buffer.
 */
static void *vq_tx_alloc_master(unsigned long *len, unsigned short *idx)
{
    return virtqueue_get_buffer(rpmsg_lite_dev.tvq, (uint32_t *) len);
}

/**
 * vq_rx_master
 *
 * Retrieves the received buffer from the virtqueue.
 *
 * @param len  - size of received buffer
 * @param idx  - index of buffer
 *
 * @return - pointer to received buffer
 *
 */
static void *vq_rx_master(unsigned long *len, unsigned short *idx)
{
    return virtqueue_get_buffer(rpmsg_lite_dev.rvq, (uint32_t*)len);
}

/**
 * vq_rx_free_master
 *
 * Places the used buffer back on the virtqueue.
 *
 * @param buffer - buffer pointer
 * @param len    - buffer length
 * @param idx    - buffer index
 *
 */
static void vq_rx_free_master(void *buffer, unsigned long len, unsigned short idx)
{
    struct llist node;
    int status;
    /* Initialize buffer node */
    node.data = buffer;
    node.attr = len;
    node.next = RL_NULL;
    node.prev = RL_NULL;

    status = virtqueue_add_buffer(rpmsg_lite_dev.rvq, &node, 0, 1, buffer);
    RL_ASSERT(status == VQUEUE_SUCCESS); /* must success here */

    /* As long as the length of the virtqueue ring buffer is not shorter
     * than the number of buffers in the pool, this function should not fail.
     * This condition is always met, so we don't need to return anything here */
}


/* Interface used in case this processor is MASTER */
static const struct virtqueue_ops master_vq_ops = 
{
    vq_tx_master,
    vq_tx_alloc_master,
    vq_rx_master,
    vq_rx_free_master,
};

/* Interface used in case this processor is REMOTE */
static const struct virtqueue_ops remote_vq_ops = 
{
    vq_tx_remote,
    vq_tx_alloc_remote,
    vq_rx_remote,
    vq_rx_free_remote,
};

/*************************************************

 mmmmmm mmmmm mmmmmmm        mm   m mmmmmmm     m
 #      #   "#   #           #"m  # #     #  #  #
 #mmmmm #mmm#"   #           # #m # #mmmmm" #"# #
 #      #        #           #  # # #      ## ##"
 #mmmmm #        #           #   ## #mmmmm #   #

**************************************************/
/**
 * rpmsg_lite_create_ept
 *
 * Create a new rpmsg endpoint, which can be used
 * for communication.
 *
 * @param addr        - desired address, RL_ADDR_ANY for automatic selection
 * @param rx_cb       - callback function called on receive
 * @param rx_cb_data  - callback data pointer, passed to rx_cb
 *
 * @return - RL_NULL on error, new endpoint pointer on success
 *
 */
struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(unsigned long addr,
                                              rl_ept_rx_cb_t rx_cb,
                                              void *rx_cb_data)
{
    struct rpmsg_lite_endpoint *rl_ept;
    struct llist *node;
    unsigned int i;

    rl_ept = env_allocate_memory(sizeof(struct rpmsg_lite_endpoint));
    if (!rl_ept)
    {
        return RL_NULL ;
    }
    env_memset(rl_ept, 0x00, sizeof(struct rpmsg_lite_endpoint));

    node = env_allocate_memory(sizeof(struct llist));
    if (!node)
    {
        env_free_memory(rl_ept);
        return RL_NULL;
    }
    
    env_lock_mutex(rpmsg_lite_dev.lock);
    {
      if(addr == RL_ADDR_ANY)
      {
        /* find lowest free address */
        for(i = 1; i < 0xFFFFFFFF; i++)
        {
          if(rpmsg_lite_get_endpoint_from_addr(i) == RL_NULL)
          {
            addr = i;
            break;
          }
        }
        if(addr == RL_ADDR_ANY)
        {
          /* no address is free, cannot happen normally */
           env_unlock_mutex(rpmsg_lite_dev.lock);
           return RL_NULL;
        }
      }
      else
      {
        if(rpmsg_lite_get_endpoint_from_addr(addr) != RL_NULL)
        {
          /* Already exists! */
           env_unlock_mutex(rpmsg_lite_dev.lock);
           return RL_NULL;
        }
      }
      
      rl_ept->addr = addr;
      rl_ept->rx_cb = rx_cb;
      rl_ept->rx_cb_data = rx_cb_data;

      node->data = rl_ept;
      
      add_to_list((struct llist **)&rpmsg_lite_dev.rl_endpoints, node);
    }
    env_unlock_mutex(rpmsg_lite_dev.lock);
    
    return rl_ept;
}
/*************************************************

 mmmmmm mmmmm mmmmmmm        mmmm   mmmmmm m     
 #      #   "#   #           #   "m #      #     
 #mmmmm #mmm#"   #           #    # #mmmmm #     
 #      #        #           #    # #      #     
 #mmmmm #        #           #mmm"  #mmmmm #mmmmm

**************************************************/
/**
 * rpmsg_lite_destroy_ept
 *
 * This function deletes rpmsg endpoint and performs cleanup.
 *
 * @param rl_ept - pointer to endpoint to destroy
 *
 */
void rpmsg_lite_destroy_ept(struct rpmsg_lite_endpoint *rl_ept)
{
    struct llist *node;
    env_lock_mutex(rpmsg_lite_dev.lock);
    node = rpmsg_lite_get_endpoint_from_addr(rl_ept->addr);
    if (node)
    {
        remove_from_list((struct llist **)&rpmsg_lite_dev.rl_endpoints, node);
        env_unlock_mutex(rpmsg_lite_dev.lock);
        env_free_memory(node);
        env_free_memory(rl_ept);
    }
    else
    {
    	env_unlock_mutex(rpmsg_lite_dev.lock);
    }
}

/******************************************

mmmmmmm m    m          mm   mmmmm  mmmmm 
   #     #  #           ##   #   "#   #   
   #      ##           #  #  #mmm#"   #   
   #     m""m          #mm#  #        #   
   #    m"  "m        #    # #      mm#mm

*******************************************/
/**
 * rpmsg_lite_send
 *
 * Sends a message contained in data field of length size
 * to the remote endpoint with address dst.
 * ept->addr is used as source address in the rpmsg header
 * of the message being sent.
 *
 * @param ept     - sender endpoint
 * @param dst     - remote endpoint address
 * @param data    - payload buffer
 * @param size    - size of payload, in bytes
 * @param timeout - timeout in ms, 0 if nonblocking
 *
 * @return - status of function execution, RL_SUCCESS on success
 *
 */
int rpmsg_lite_send(struct rpmsg_lite_endpoint *ept, unsigned long dst,
                     char *data, int size, int timeout)
{
    struct rpmsg_std_hdr *rpmsg_hdr;
    void *buffer;
    unsigned short idx;
    int tick_count = 0;
    unsigned long buff_len;
    unsigned long src;

    if(!ept)
      return RL_ERR_PARAM;
    
    if(!rpmsg_lite_dev.link_state)
      return RL_NOT_READY;
    
    //FIXME : may be just copy the data size equal to buffer length and Tx it.
    if (size > (RL_BUFFER_SIZE - sizeof(struct rpmsg_std_hdr)))
        return RL_ERR_BUFF_SIZE;
    
    src = ept->addr;

    /* Lock the device to enable exclusive access to virtqueues */
    env_lock_mutex(rpmsg_lite_dev.lock);
    /* Get rpmsg buffer for sending message. */
    buffer = rpmsg_lite_dev.vq_ops->vq_tx_alloc(&buff_len, &idx);
    env_unlock_mutex(rpmsg_lite_dev.lock);
    
    if (!buffer && !timeout)
        return RL_ERR_NO_MEM;

    while (!buffer)
    {
        env_sleep_msec(RL_MS_PER_INTERVAL);
        env_lock_mutex(rpmsg_lite_dev.lock);
        buffer = rpmsg_lite_dev.vq_ops->vq_tx_alloc(&buff_len, &idx);
        env_unlock_mutex(rpmsg_lite_dev.lock);
        tick_count += RL_MS_PER_INTERVAL;
        if ((tick_count >= timeout) && (!buffer))
        {
            return RL_ERR_NO_MEM;
        }
    }
    
    rpmsg_hdr = (struct rpmsg_std_hdr *) buffer;

    /* Initialize RPMSG header. */
    rpmsg_hdr->dst = dst;
    rpmsg_hdr->src = src;
    rpmsg_hdr->len = size;
    rpmsg_hdr->flags = 0;

    /* Copy data to rpmsg buffer. */
    env_memcpy(rpmsg_hdr->data, data, size);

    env_lock_mutex(rpmsg_lite_dev.lock);
    /* Enqueue buffer on virtqueue. */
    rpmsg_lite_dev.vq_ops->vq_tx(buffer, buff_len, idx);
    /* Let the other side know that there is a job to process. */
    virtqueue_kick(rpmsg_lite_dev.tvq);
    env_unlock_mutex(rpmsg_lite_dev.lock);
    
    return RL_SUCCESS;
}

#if defined(RL_API_HAS_ZEROCOPY) && (RL_API_HAS_ZEROCOPY == 1)
/*!
 * @brief Allocates the tx buffer for message payload.
 *
 * This API can only be called at process context to get the tx buffer in vring. By this way, the
 * application can directly put its message into the vring tx buffer without copy from an application buffer.
 * It is the application responsibility to correctly fill the allocated tx buffer by data and passing correct
 * parameters to the rpmsg_lite_send_nocopy() function to perform data no-copy-send mechanism.
 *
 * @param[in] size     Pointer to store tx buffer size
 * @param[in] timeout  Integer, wait upto timeout ms or not for buffer to become available
 *
 * @return The tx buffer address on success and NULL on failure
 * 
 * @see rpmsg_lite_send_nocopy
 */
void *rpmsg_lite_alloc_tx_buffer(unsigned long *size, int timeout)
{
    struct rpmsg_std_hdr *rpmsg_hdr;
    void *buffer;
    unsigned short idx;
    int tick_count = 0;

    if(!size)
        return NULL;
    
    if(!rpmsg_lite_dev.link_state)
    {
        *size = 0;
          return NULL;
    }

    /* Lock the device to enable exclusive access to virtqueues */
    env_lock_mutex(rpmsg_lite_dev.lock);
    /* Get rpmsg buffer for sending message. */
    buffer = rpmsg_lite_dev.vq_ops->vq_tx_alloc(size, &idx);
    env_unlock_mutex(rpmsg_lite_dev.lock);
    
    if (!buffer && !timeout)
    {
        *size = 0;
        return NULL;
    }

    while (!buffer)
    {
        env_sleep_msec(RL_MS_PER_INTERVAL);
        env_lock_mutex(rpmsg_lite_dev.lock);
        buffer = rpmsg_lite_dev.vq_ops->vq_tx_alloc(size, &idx);
        env_unlock_mutex(rpmsg_lite_dev.lock);
        tick_count += RL_MS_PER_INTERVAL;
        if ((tick_count >= timeout) && (!buffer))
        {
            *size = 0;
            return NULL;
        }
    }
    
    rpmsg_hdr = (struct rpmsg_std_hdr *) buffer;
       return rpmsg_hdr->data;
}

/*!
 * @brief Sends a message in tx buffer allocated by rpmsg_lite_alloc_tx_buffer() 
 * 
 * This function sends txbuf of length len to the remote dst address,
 * and uses ept->addr as the source address.
 * The application has to take the responsibility for:
 *  1. tx buffer allocation (rpmsg_lite_alloc_tx_buffer())
 *  2. filling the data to be sent into the pre-allocated tx buffer
 *  3. not exceeding the buffer size when filling the data
 *  4. data cache coherency
 *
 * After the rpmsg_lite_send_nocopy() function is issued the tx buffer is no more owned 
 * by the sending task and must not be touched anymore unless the rpmsg_lite_send_nocopy() 
 * function fails and returns an error. 
 *
 * @param[in] ept   Sender endpoint pointer
 * @param[in] dst   Destination address
 * @param[in] data  TX buffer with message filled
 * @param[in] size  Length of payload
 *
 * @return 0 on success and an appropriate error value on failure
 * 
 * @see rpmsg_lite_alloc_tx_buffer
 */
int rpmsg_lite_send_nocopy(struct rpmsg_lite_endpoint *ept, unsigned long dst,
                                    void *data, int size)
{
    struct rpmsg_std_hdr *rpmsg_hdr;
    unsigned long src;
    struct rpmsg_hdr_reserved * reserved = RL_NULL;

    if(!ept || !data)
      return RL_ERR_PARAM;
    
    if(!rpmsg_lite_dev.link_state)
      return RL_NOT_READY;

    if (size > (RL_BUFFER_SIZE - sizeof(struct rpmsg_std_hdr)))
        return RL_ERR_BUFF_SIZE;
    
    src = ept->addr;
    
    rpmsg_hdr = RPMSG_STD_HDR_FROM_BUF(data);

    /* Initialize RPMSG header. */
    rpmsg_hdr->dst = dst;
    rpmsg_hdr->src = src;
    rpmsg_hdr->len = size;
    rpmsg_hdr->flags = 0;

    reserved = (struct rpmsg_hdr_reserved*)&rpmsg_hdr->reserved;

    env_lock_mutex(rpmsg_lite_dev.lock);
    /* Enqueue buffer on virtqueue. */
    rpmsg_lite_dev.vq_ops->vq_tx(data,  (unsigned long)reserved->totlen, reserved->idx);
    /* Let the other side know that there is a job to process. */
    virtqueue_kick(rpmsg_lite_dev.tvq);
    env_unlock_mutex(rpmsg_lite_dev.lock);
    
    return RL_SUCCESS;    
}

/******************************************
 
 mmmmm  m    m          mm   mmmmm  mmmmm 
 #   "#  #  #           ##   #   "#   #   
 #mmmm"   ##           #  #  #mmm#"   #   
 #   "m  m""m          #mm#  #        #   
 #    " m"  "m        #    # #      mm#mm

 *******************************************/
/**
 * rpmsg_lite_release_rx_buffer 
 *
 * Releases the rx buffer for future reuse in vring.
 * This API can be called at process context when the
 * message in rx buffer is processed.
 * 
 * @param rxbuf - rx buffer with message payload
 * 
 */
void rpmsg_lite_release_rx_buffer(void *rxbuf)
{
    struct rpmsg_std_hdr *rpmsg_hdr;
    struct rpmsg_hdr_reserved * reserved = RL_NULL;

    if (!rxbuf)
        return;

    rpmsg_hdr = RPMSG_STD_HDR_FROM_BUF(rxbuf);

    /* Get the pointer to the reserved field that contains buffer size and the index */
    reserved = (struct rpmsg_hdr_reserved*)&rpmsg_hdr->reserved;

    env_lock_mutex(rpmsg_lite_dev.lock);

    /* Return used buffer, with total length (header length + buffer size). */
    rpmsg_lite_dev.vq_ops->vq_rx_free(rpmsg_hdr, (unsigned long)reserved->totlen, reserved->idx);

    env_unlock_mutex(rpmsg_lite_dev.lock);
}

#endif /* RL_API_HAS_ZEROCOPY */

/******************************

 mmmmm  mm   m mmmmm mmmmmmm
   #    #"m  #   #      #   
   #    # #m #   #      #   
   #    #  # #   #      #   
 mm#mm  #   ## mm#mm    #

 *****************************/
 /**
 * rpmsg_lite_init
 *
 * Initializes the RPMSG lite communication stack.
 * Must be called prior to any other RPMSG lite API. 
 *
 * @param dev_id        - id passed to platform_get_processor_by_id()
 *                        to obtain this processor description.
 * @param remote_role   - role of the remote processor
 *
 *
 * @return - execution status, RL_SUCCESS on success
 *
 */
int rpmsg_lite_init(int dev_id, int remote_role)
{
    struct virtio_device *virt_dev;
    int status;
    void (*callback[2])(struct virtqueue *vq);
    const char *vq_names[2];
    struct hil_proc* proc_ptr = RL_NULL;
    struct vring_alloc_info ring_info;
    struct virtqueue *vqs[RL_MAX_VQ_PER_RDEV];
    void *buffer;
    struct llist node;
    int idx, num_vrings, j;
    
    status = env_init();
    if (status != RL_SUCCESS) 
        return RL_ERR_PARAM;
    
    proc_ptr = platform_get_processor_by_id(dev_id);
    if(proc_ptr == RL_NULL)
        return RL_ERR_DEV_ID;

    /* Get the vring HW info for the given virtio device */
    num_vrings = proc_ptr->vdev.num_vrings;

    if (num_vrings > RL_MAX_VQ_PER_RDEV)
        return RL_ERR_MAX_VQ;

    rpmsg_lite_dev.remote_role = remote_role;
    
    /* Initialize the virtio device */
    virt_dev = (struct virtio_device *)&rpmsg_lite_dev.virt_dev;
    virt_dev->device = proc_ptr;

    /* no accessing, not using, not implemented, lightweight */
    virt_dev->func = RL_NULL;
    

    if (remote_role == RL_REMOTE) 
    {
        /*
         * Since device is RPMSG Remote so we need to manage the
         * shared buffers. Create shared memory pool to handle buffers.
         */
        rpmsg_lite_dev.sh_mem_base = (char *)RL_WORD_ALIGN_UP(proc_ptr->sh_buff.start_addr);
        rpmsg_lite_dev.sh_mem_remaining = (RL_WORD_ALIGN_DOWN(proc_ptr->sh_buff.size)) / RL_BUFFER_SIZE;
        rpmsg_lite_dev.sh_mem_total = rpmsg_lite_dev.sh_mem_remaining;

        if (!rpmsg_lite_dev.sh_mem_base)
            return RL_ERR_NO_MEM;

        /* Initialize names and callbacks*/
        vq_names[0] = "rx_vq";
        vq_names[1] = "tx_vq";
        callback[0] = rpmsg_lite_rx_callback;
        callback[1] = rpmsg_lite_tx_callback;
        rpmsg_lite_dev.vq_ops = &master_vq_ops;
    }
    else
    {
        vq_names[0] = "tx_vq"; /* swapped in case of remote */
        vq_names[1] = "rx_vq";
        callback[0] = rpmsg_lite_tx_callback;
        callback[1] = rpmsg_lite_rx_callback;
        rpmsg_lite_dev.vq_ops = &remote_vq_ops;
    }

    /* Create virtqueue for each vring. */
    for (idx = 0; idx < num_vrings; idx++) 
    {
        ring_info.phy_addr = proc_ptr->vdev.vring_info[idx].phy_addr;
        ring_info.align = proc_ptr->vdev.vring_info[idx].align;
        ring_info.num_descs = proc_ptr->vdev.vring_info[idx].num_descs;

        if (remote_role == RL_REMOTE)
            env_memset((void*) ring_info.phy_addr, 0x00,
                        vring_size(ring_info.num_descs, ring_info.align));

        status = virtqueue_create(virt_dev, idx, (char *) vq_names[idx], &ring_info,
                        callback[idx], hil_vring_notify,
                        &vqs[idx]);

        if (status != RL_SUCCESS)
            return status;
    }

    status = env_create_mutex((LOCK *)&rpmsg_lite_dev.lock, 1);
    if (status != RL_SUCCESS)
        return status;

    //FIXME - a better way to handle this , tx for master is rx for remote and vice versa.
    if (remote_role == RL_REMOTE)
    {
        rpmsg_lite_dev.tvq = vqs[1];
        rpmsg_lite_dev.rvq = vqs[0];

        for (j = 0; j < RL_MAX_VQ_PER_RDEV; j++)
        {
            for (idx = 0; ((idx < vqs[j]->vq_nentries)
                            && (idx < rpmsg_lite_dev.sh_mem_total));
                            idx++)
            {

                /* Initialize TX virtqueue buffers for remote device */
                buffer = rpmsg_lite_dev.sh_mem_remaining? 
                (rpmsg_lite_dev.sh_mem_base + RL_BUFFER_SIZE*(rpmsg_lite_dev.sh_mem_total-rpmsg_lite_dev.sh_mem_remaining--)) :
                (RL_NULL);

                RL_ASSERT(buffer);

                node.data = buffer;
                node.attr = RL_BUFFER_SIZE;
                node.next = RL_NULL;

                env_memset(buffer, 0x00, RL_BUFFER_SIZE);
                if (vqs[j] == rpmsg_lite_dev.rvq)
                    status = virtqueue_add_buffer(vqs[j], &node, 0, 1, buffer);
                else if (vqs[j] == rpmsg_lite_dev.tvq)
                    status = virtqueue_fill_used_buffers(vqs[j], &node, RL_BUFFER_SIZE, buffer);
                else
                    RL_ASSERT(0); /* should not happen */


                if (status != RL_SUCCESS)
                {
                    /* Clean up! */
                    env_delete_mutex(rpmsg_lite_dev.lock);
                    return status;
                }
            }
        }  

        /* Enable ISRs */
        hil_enable_vring_notifications(0, rpmsg_lite_dev.rvq);
        hil_enable_vring_notifications(1, rpmsg_lite_dev.tvq);
        env_disable_interrupts();
        rpmsg_lite_dev.link_state = 1;
        env_restore_interrupts();
      
        /*
         * Let the remote device know that Master is ready for
         * communication.
         */
        virtqueue_kick(rpmsg_lite_dev.rvq);    
    }
    else
    {
        rpmsg_lite_dev.tvq = vqs[0];
        rpmsg_lite_dev.rvq = vqs[1];

        /* Enable ISRs */
        hil_enable_vring_notifications(0, rpmsg_lite_dev.tvq);
        hil_enable_vring_notifications(1, rpmsg_lite_dev.rvq);
        env_disable_interrupts();
        rpmsg_lite_dev.link_state = 0;
        env_restore_interrupts();
    }

    return status;
}


/*******************************************

 mmmm   mmmmmm mmmmm  mm   m mmmmm mmmmmmm
 #   "m #        #    #"m  #   #      #   
 #    # #mmmmm   #    # #m #   #      #   
 #    # #        #    #  # #   #      #   
 #mmm"  #mmmmm mm#mm  #   ## mm#mm    #

********************************************/
/**
 * rpmsg_lite_deinit
 *
 * Deinitialized the RPMSG lite communication stack
 * This function always succeeds.
 * rpmsg_lite_init() can be called again after this
 * function has been called.
 *
 */
void rpmsg_lite_deinit(void)
{
    env_disable_interrupts();
    rpmsg_lite_dev.link_state = 0;
    env_restore_interrupts();
    
    if (rpmsg_lite_dev.remote_role == RL_MASTER)
    {
        if (rpmsg_lite_dev.rvq)
        {
            hil_disable_vring_notifications(1, rpmsg_lite_dev.rvq);
            virtqueue_free(rpmsg_lite_dev.rvq);
        }
        if (rpmsg_lite_dev.tvq)
        {
            hil_disable_vring_notifications(0, rpmsg_lite_dev.tvq);
            virtqueue_free(rpmsg_lite_dev.tvq);
        }
    }
    else
    { 
        if (rpmsg_lite_dev.rvq)
        {
            hil_disable_vring_notifications(0, rpmsg_lite_dev.rvq);
            virtqueue_free(rpmsg_lite_dev.rvq);
        }
        if (rpmsg_lite_dev.tvq)
        {
            hil_disable_vring_notifications(1, rpmsg_lite_dev.tvq);
            virtqueue_free(rpmsg_lite_dev.tvq);
        }
    }  

    if (rpmsg_lite_dev.lock)
    {
        env_delete_mutex(rpmsg_lite_dev.lock);
    }
    
    env_deinit();
}

