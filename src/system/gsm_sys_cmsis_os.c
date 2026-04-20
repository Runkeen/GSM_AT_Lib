/**	
 * \file            gsm_sys_cmsis_os.c
 * \brief           System dependant functions for STM32 MCU with CMSIS OS
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
#include "gsm_ll_optimal.h"
#include "system/gsm_sys_cmsis_os.h"
#include "system/gsm_sys.h"
#include "cmsis_os.h"
#include "gsm/gsm_private.h"

#if !__DOXYGEN__

static gsm_sys_mutex_t sys_mutex;              /* Mutex ID for main protection */

uint8_t
gsm_sys_init(void) {
    gsm_sys_mutex_create(&sys_mutex);           /* Create system mutex */
    return 1;
}

uint32_t
gsm_sys_now(void) {
    return osKernelSysTick();                   /* Get current tick in units of milliseconds */
}

uint8_t
gsm_sys_protect(void) {
    gsm_sys_mutex_lock(&sys_mutex);             /* Lock system and protect it */
    return 1;
}

uint8_t
gsm_sys_unprotect(void) {
    gsm_sys_mutex_unlock(&sys_mutex);           /* Release lock */
    return 1;
}

uint8_t
gsm_sys_mutex_create(gsm_sys_mutex_t* p) {
#if (osCMSIS < 0x20000U)
    osMutexDef(MUT);                            /* Define a mutex */
    *p = osRecursiveMutexCreate(osMutex(MUT));  /* Create recursive mutex */
    return *p != NULL;                          /* Return status */
#else
    const osMutexAttr_t Thread_Mutex_attr = {
        "GSM_Thread_Mutex",                       // human readable mutex name
        osMutexRecursive | osMutexPrioInherit,    // attr_bits
        NULL,                                     // memory for control block
        0U                                        // size for control block
        };
    *p = osMutexNew(&Thread_Mutex_attr);
    return *p != NULL;
#endif
}

uint8_t
gsm_sys_mutex_delete(gsm_sys_mutex_t* p) {
    return osMutexDelete(*p) == osOK;           /* Delete mutex */
}

uint8_t
gsm_sys_mutex_lock(gsm_sys_mutex_t* p) {
#if (osCMSIS < 0x20000U)
    return osRecursiveMutexWait(*p, osWaitForever) == osOK; /* Wait forever for mutex */
#else
    return osMutexAcquire(*p, osWaitForever) == osOK; /* Wait forever for mutex */
#endif
}

uint8_t
gsm_sys_mutex_unlock(gsm_sys_mutex_t* p) {
#if (osCMSIS < 0x20000U)
    return osRecursiveMutexRelease(*p) == osOK; /* Release mutex */
#else
    return osMutexRelease(*p);
#endif
}


uint8_t
gsm_sys_mutex_isvalid(gsm_sys_mutex_t* p) {
    return *p != NULL;                          /* Check if mutex is valid */
}

uint8_t
gsm_sys_mutex_invalid(gsm_sys_mutex_t* p) {
    *p = GSM_SYS_MUTEX_NULL;                    /* Set mutex as invalid */
    return 1;
}

uint8_t
gsm_sys_sem_create(gsm_sys_sem_t* p, uint8_t cnt) {
#if (osCMSIS < 0x20000U)
    osSemaphoreDef(SEM);                        /* Define semaphore info */
    *p = osSemaphoreCreate(osSemaphore(SEM), 1);/* Create semaphore with one token */
#else
    *p = osSemaphoreNew(16, cnt, NULL);/* Create semaphore with cnt token */
#endif

    if (*p != NULL && !cnt) {                   /* We have valid entry */
#if (osCMSIS < 0x20000U)
        osSemaphoreWait(*p, 0);                 /* Lock semaphore immediatelly */
#else
        osSemaphoreAcquire(p, 0);
#endif
    }
    return *p != NULL;
}

uint8_t
gsm_sys_sem_delete(gsm_sys_sem_t* p) {
    return osSemaphoreDelete(*p) == osOK;       /* Delete semaphore */
}

uint32_t
gsm_sys_sem_wait(gsm_sys_sem_t* p, uint32_t timeout) {
    uint32_t tick = osKernelSysTick();          /* Get start tick time */
#if (osCMSIS < 0x20000U)
    return (osSemaphoreWait(*p, !timeout ? osWaitForever : timeout) == osOK) ? (osKernelSysTick() - tick) : GSM_SYS_TIMEOUT;    /* Wait for semaphore with specific time */
#else
    return (osSemaphoreAcquire(*p, !timeout ? osWaitForever : timeout) == osOK) ? (osKernelSysTick() - tick) : GSM_SYS_TIMEOUT;    /* Wait for semaphore with specific time */
#endif
}

uint8_t
gsm_sys_sem_release(gsm_sys_sem_t* p) {
    return osSemaphoreRelease(*p) == osOK;      /* Release semaphore */
}

uint8_t
gsm_sys_sem_isvalid(gsm_sys_sem_t* p) {
    return *p != NULL;                          /* Check if valid */
}

uint8_t
gsm_sys_sem_invalid(gsm_sys_sem_t* p) {
    *p = GSM_SYS_SEM_NULL;                      /* Invaldiate semaphore */
    return 1;
}

uint8_t
gsm_sys_mbox_create(gsm_sys_mbox_t* b, size_t size) {
#if (osCMSIS < 0x20000U)
    osMessageQDef(MBOX, size, void *);          /* Define message box */
    *b = osMessageCreate(osMessageQ(MBOX), NULL);   /* Create message box */
#else
    uint32_t msg_sz = sizeof(gsm_msg_t);
    *b = osMessageQueueNew(size, msg_sz, NULL);   /* Create message box */
#endif
    return *b != NULL;
}

uint8_t
gsm_sys_mbox_delete(gsm_sys_mbox_t* b) {
#if (osCMSIS < 0x20000U)
    if (osMessageWaiting(*b)) {                 /* We still have messages in queue, should not delete queue */
        return 0;                               /* Return error as we still have entries in message queue */
    }
    return osMessageDelete(*b) == osOK;         /* Delete message queue */
#else
    if (osMessageQueueGetCount(*b))
        return 0;
    return osMessageQueueDelete(*b) == osOK;         /* Delete message queue */
#endif
}

uint32_t
gsm_sys_mbox_put(gsm_sys_mbox_t* b, void* m) {
    uint32_t tick = osKernelSysTick();          /* Get start time */
#if (osCMSIS < 0x20000U)
    return osMessagePut(*b, (uint32_t)m, osWaitForever) == osOK ? (osKernelSysTick() - tick) : GSM_SYS_TIMEOUT; /* Put new message with forever timeout */
#else
    return osMessageQueuePut(*b, m, osPriorityNormal, 0U) == osOK ?
        (osKernelSysTick() - tick) : GSM_SYS_TIMEOUT; /* Put new message with forever timeout */
#endif
}

uint32_t
gsm_sys_mbox_get(gsm_sys_mbox_t* b, void* m, uint32_t timeout) {
    uint32_t time = osKernelSysTick();          /* Get current time */
#if (osCMSIS < 0x20000U)
    osEvent evt;
    evt = osMessageGet(*b, !timeout ? osWaitForever : timeout); /* Get message event */
    if (evt.status == osEventMessage) {         /* Did we get a message? */
        *m = evt.value.p;                       /* Set value */
        return osKernelSysTick() - time;        /* Return time required for reading message */
    }
#else
    osStatus_t stat;  // TODO  - проверить, что в m записывается адрес сообщения
    stat = osMessageQueueGet(*b, m, NULL, !timeout ? osWaitForever : timeout); /* Get message event */
    
    if (stat == osOK) {
        return osKernelSysTick() - time;        /* Return time required for reading message */
    }
    m = NULL;
#endif
    return GSM_SYS_TIMEOUT;
}

uint8_t
gsm_sys_mbox_putnow(gsm_sys_mbox_t* b, void* m) {
#if (osCMSIS < 0x20000U)
    return osMessagePut(*b, (uint32_t)m, 0) == osOK;   /* Put new message without timeout */
#else
    osStatus_t ret = osMessageQueuePut(*b, m, osPriorityNormal, 0) == osOK;   /* Put new message without timeout */
    return ret;
#endif
}

uint8_t
gsm_sys_mbox_getnow(gsm_sys_mbox_t* b, void** m) {
#if (osCMSIS < 0x20000U)
    osEvent evt;

    evt = osMessageGet(*b, 0);                  /* Get message event */
    if (evt.status == osEventMessage) {         /* Did we get a message? */
        *m = evt.value.p;                       /* Set value */
        return 1;
    }
#else
    osStatus_t stat;
    stat = osMessageQueueGet(*b, m, NULL, 0); /* Get message event */
    if (stat == osOK)
        return 1;
#endif
    return 0;
}

uint8_t
gsm_sys_mbox_isvalid(gsm_sys_mbox_t* b) {
    return *b != NULL;                          /* Return status if message box is valid */
}

uint8_t
gsm_sys_mbox_invalid(gsm_sys_mbox_t* b) {
    *b = GSM_SYS_MBOX_NULL;                     /* Invalidate message box */
    return 1;
}

struct gsm_ctx* gsm_ctx = NULL;

uint8_t
gsm_sys_thread_create(gsm_sys_thread_t* t, const char* name, gsm_sys_thread_fn thread_func,
     void* const arg, size_t stack_size, gsm_sys_thread_prio_t prio) {
    gsm_sys_thread_t id;
    osThreadAttr_t attr;
    memset(&attr, 0, sizeof(attr));
#if (osCMSIS < 0x20000U)
    const osThreadDef_t thread_def = {(char *)name, (os_pthread)thread_func, (osPriority)prio, 0, stack_size ? stack_size : GSM_SYS_THREAD_SS };    /* Create thread description */
    id = osThreadCreate(&thread_def, arg);      /* Create thread */
#else
    if ((gsm_ctx != NULL) && (gsm_ctx->stack_type == STACK_TYPE_STATIC)) {
        uint8_t n = gsm_ctx->stack_num;
        // Attributes Definitions for Tasks
        attr.name =  (char *)name;
        attr.stack_size = stack_size; // * 4;
        attr.stack_mem = &gsm_ctx->stack[n].buf[0];
        attr.cb_mem = &gsm_ctx->stack[n].tcb;
        attr.cb_size = sizeof(StaticTask_t);
        attr.priority = (osPriority_t) prio;
        gsm_ctx->stack_num++;
    }
    else {
        attr.name =  (char *)name;
        attr.stack_size = stack_size; // * 4;
        attr.priority = (osPriority_t) prio;
    }
    id = osThreadNew(*thread_func, arg, &attr);      /* Create thread */
#endif
    if (t != NULL) {
        *t = id;
    }
    return id != NULL;
}

uint8_t
gsm_sys_thread_terminate(gsm_sys_thread_t* t) {
    osThreadTerminate(t != NULL ? *t : NULL);   /* Terminate thread */
    return 1;
}

uint8_t
gsm_sys_thread_yield(void) {
    osThreadYield();                            /* Yield current thread */
    return 1;
}

uint8_t gsm_ctx_set(struct gsm_ctx* ctx) {
    if (ctx != NULL) {
        gsm_ctx = ctx;
        return 1;
    }
    return 0;
}

#endif /* !__DOXYGEN__ */
