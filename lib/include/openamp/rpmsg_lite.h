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

#ifndef _RPMSG_LITE_H
#define _RPMSG_LITE_H

#include <stddef.h>
#include "openamp/env.h"
#include "openamp/virtio.h"
#include "openamp/hil.h"
#include "openamp/llist.h"


/* CONFIGURABLE PARAMETERS*/
/* START { */
#define RL_MS_PER_INTERVAL                   (10)
#define RL_BUFFER_SIZE                       (512)
#define RL_ASSERT(x)  do{if(!x)while(1);}while(0);
#define RL_API_HAS_ZEROCOPY                  (1)
/* } END */

 /* Shared memory "allocator" parameters */
#define RL_WORD_SIZE                (sizeof(unsigned long))
#define RL_WORD_ALIGN_UP(a)            (((((unsigned long)a) & (RL_WORD_SIZE-1)) != 0)? \
                                       ((((unsigned long)a) & (~(RL_WORD_SIZE-1))) + 4):((unsigned long)a))
#define RL_WORD_ALIGN_DOWN(a)            (((((unsigned long)a) & (RL_WORD_SIZE-1)) != 0)? \
                                       (((unsigned long)a) & (~(RL_WORD_SIZE-1))):((unsigned long)a))

/* Definitions for device types , null pointer, etc.*/
#define RL_SUCCESS                           0
#define RL_NULL                              (void *)0
#define RL_REMOTE                            0
#define RL_MASTER                            1
#define RL_TRUE                              1
#define RL_FALSE                             0
#define RL_ADDR_ANY                     (0xFFFFFFFF)
#define RL_RELEASE                           (0)
#define RL_HOLD                              (1)
#define RL_DONT_BLOCK                        (0)
#define RL_MAX_VQ_PER_RDEV                   (2)

/* Error macros. */
#define RL_ERRORS_BASE                       -5000
#define RL_ERR_NO_MEM                        (RL_ERRORS_BASE - 1)
#define RL_ERR_BUFF_SIZE                     (RL_ERRORS_BASE - 2)
#define RL_ERR_PARAM                         (RL_ERRORS_BASE - 3)
#define RL_ERR_DEV_ID                        (RL_ERRORS_BASE - 4)
#define RL_ERR_MAX_VQ                        (RL_ERRORS_BASE - 5)
#define RL_ERR_NO_BUFF                       (RL_ERRORS_BASE - 6)
#define RL_NOT_READY                         (RL_ERRORS_BASE - 7)


#if defined(__IAR_SYSTEMS_ICC__)
__packed
#endif
/**
 * struct rpmsg_std_hdr - common header for all rpmsg messages
 * @src: source endpoint address
 * @dst: destination endpoint address
 * @reserved: reserved for future use
 * @len: length of payload (in bytes)
 * @flags: message flags
 * @data: @len bytes of message payload data
 *
 * Every message sent(/received) on the rpmsg bus begins with this header.
 */
struct rpmsg_std_hdr
{
    unsigned long src;
    unsigned long dst;
    unsigned long reserved;
    unsigned short len;
    unsigned short flags;
    unsigned char data[1];
#if defined(__IAR_SYSTEMS_ICC__)
};
#else
}__attribute__((packed));
#endif

/* Up to 16 flags available */
enum rpmsg_std_flags
{
    RL_REMOTE_UP            = 0,
    RL_REMOTE_DYING         = 1,
    RL_DROP_HDR_FLAG        = 2,
    RL_FRAGMENTED           = 3, /* future proposal, for messages > MTU */
    RL_LAST_FRAGMENT        = 4, /* future proposal, for messages > MTU */
};

typedef int (*rl_ept_rx_cb_t)(void *payload, int payload_len,
                               unsigned long src, void *priv);

struct rpmsg_lite_endpoint
{
    unsigned long addr;
    rl_ept_rx_cb_t rx_cb;
    void *rx_cb_data;
    void *rfu; /* reserved for future usage */
    /* 16 bytes aligned on 32bit architecture */
};


/* Exported API functions */
int rpmsg_lite_init(int dev_id, int remote_role);
void rpmsg_lite_deinit(void);
struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(unsigned long addr,
                                                    rl_ept_rx_cb_t rx_cb,
                                                    void *rx_cb_data);
void rpmsg_lite_destroy_ept(struct rpmsg_lite_endpoint *rl_ept);
int rpmsg_lite_send(struct rpmsg_lite_endpoint *ept, unsigned long dst,
                     char *data, int size, int timeout);

#if defined(RL_API_HAS_ZEROCOPY) && (RL_API_HAS_ZEROCOPY == 1)
void rpmsg_lite_release_rx_buffer(void *rxbuf);
void *rpmsg_lite_alloc_tx_buffer(unsigned long *size, int timeout);
int rpmsg_lite_send_nocopy(struct rpmsg_lite_endpoint *ept, unsigned long dst,
                                    void *data, int size);
#endif

#endif /* _RPMSG_LITE_H */