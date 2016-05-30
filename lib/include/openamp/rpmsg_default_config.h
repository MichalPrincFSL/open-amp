/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
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

/*
 * -------------------------------------------------------------
 * This file provides default RPMSG configuration and its
 * validation. Parameters can be overriden in rpmsg_config.h.
 * Please define symbol RPMSG_HAS_APP_SPECIFIC_CONFIG prior to
 * providing rpmsg_config.h application specific configuration.
 * -------------------------------------------------------------
 */

#ifndef _RPMSG_DEFAULT_CONFIG_H_
#define _RPMSG_DEFAULT_CONFIG_H_

#ifdef RPMSG_HAS_APP_SPECIFIC_CONFIG
#include "rpmsg_config.h" 
#endif


#ifndef RPMSG_BUFFER_SIZE
#define RPMSG_BUFFER_SIZE                       512
#endif

#ifndef RPMSG_MAX_VQ_PER_RDEV
#define RPMSG_MAX_VQ_PER_RDEV                   2
#endif

#ifndef RPMSG_NS_EPT_ADDR
#define RPMSG_NS_EPT_ADDR                       0x35
#endif

#ifndef RPMSG_ADDR_BMP_SIZE
#define RPMSG_ADDR_BMP_SIZE                     4
#endif

/* Total tick count for 15secs - 1msec tick. */
#ifndef RPMSG_TICK_COUNT
#define RPMSG_TICK_COUNT                        15000
#endif

/* Time to wait - In multiple of 10 msecs. */
#ifndef RPMSG_TICKS_PER_INTERVAL
#define RPMSG_TICKS_PER_INTERVAL                10
#endif

/* IPI_VECT here defines VRING index in MU */
#ifndef VRING0_IPI_VECT
#define VRING0_IPI_VECT                   0
#endif

#ifndef VRING1_IPI_VECT
#define VRING1_IPI_VECT                   1
#endif

#ifndef MASTER_CPU_ID
#define MASTER_CPU_ID                     0
#endif

#ifndef REMOTE_CPU_ID
#define REMOTE_CPU_ID                     1
#endif

#ifndef RPMSG_RECV_NOCOPY_CHECK_PTRS
#define RPMSG_RECV_NOCOPY_CHECK_PTRS (1)
#endif


#endif /* _RPMSG_DEFAULT_CONFIG_H_ */
