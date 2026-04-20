/**	
 * \file            gsm_sys_cmsis_os.h
 * \brief           CMSIS-OS based system file
 */

/*
 * Copyright (c) 2018 Tilen Majerle
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of GSM-AT library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 */
#ifndef GSM_HDR_SYSTEM_CMSIS_OS_H
#define GSM_HDR_SYSTEM_CMSIS_OS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "stdint.h"
#include "stdlib.h"

#include "gsm/gsm_config.h"

#if GSM_CFG_OS && !__DOXYGEN__
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "arch/sys_arch.h"

#define osWaitForever     0xFFFFFFFF     ///< wait forever timeout value

/// \details Memory Pool ID identifies the memory pool.
typedef int osPriority;

typedef sys_mutex_t             gsm_sys_mutex_t;
typedef sys_sem_t               gsm_sys_sem_t;
typedef sys_mbox_t              gsm_sys_mbox_t;
typedef sys_thread_t            gsm_sys_thread_t;
typedef osPriority              gsm_sys_thread_prio_t;
#define GSM_SYS_MBOX_NULL           (gsm_sys_mbox_t*) NULL
#define GSM_SYS_SEM_NULL            (gsm_sys_sem_t*) NULL
#define GSM_SYS_MUTEX_NULL          (sys_mutex_t*) NULL
#define GSM_SYS_THREAD_NULL         (gsm_sys_thread_t*) NULL
#define GSM_SYS_TIMEOUT             ((uint32_t)osWaitForever)
#define GSM_SYS_THREAD_PRIO         (0)
#define GSM_SYS_THREAD_SS           (1024)

#include "gsm_sys.h"

#endif /* GSM_CFG_OS && !__DOXYGEN__ */

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* GSM_HDR_SYSTEM_CMSIS_OS_H */
