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
#include "system/gsm_sys_freertos.h"
#include "system/gsm_sys.h"
#include "gsm/gsm_private.h"
#include "lwip/sys.h"

#if !__DOXYGEN__

static gsm_sys_mutex_t sys_mutex;               /* Mutex ID for main protection */

uint8_t
gsm_sys_init(void) {
    gsm_sys_mutex_create(&sys_mutex);           /* Create system mutex */
    return 1;
}

uint32_t
gsm_sys_now(void) {
    return sys_now();                   /* Get current tick in units of milliseconds */
}

uint8_t
gsm_sys_protect(void) {
    gsm_sys_mutex_lock(&sys_mutex);     /* Lock system and protect it */
    return 1;
}

uint8_t
gsm_sys_unprotect(void) {
    gsm_sys_mutex_unlock(&sys_mutex);   /* Release lock */
    return 1;
}

uint8_t
gsm_sys_mutex_create(gsm_sys_mutex_t* p) {
    sys_mutex_new(p);           /* Create recursive mutex */
    return p != NULL;           /* Return status */
}

uint8_t
gsm_sys_mutex_delete(gsm_sys_mutex_t* p) {
    sys_mutex_free((sys_mutex_t*) p);     /* Delete mutex */
    return 0;
}

uint8_t
gsm_sys_mutex_lock(gsm_sys_mutex_t* p) {
    sys_mutex_lock((sys_mutex_t*) p);
    return 1;
}

uint8_t
gsm_sys_mutex_unlock(gsm_sys_mutex_t* p) {
    sys_mutex_unlock((sys_mutex_t*) p);   /* Release mutex */
    return 1;
}

uint8_t
gsm_sys_mutex_isvalid(gsm_sys_mutex_t* p) {
    return p != NULL;                          /* Check if mutex is valid */
}

uint8_t
gsm_sys_mutex_invalid(gsm_sys_mutex_t* p) {
    p = GSM_SYS_MUTEX_NULL;                    /* Set mutex as invalid */
    return 1;
}

uint8_t
gsm_sys_sem_create(gsm_sys_sem_t* p, uint8_t cnt) {
    sys_sem_new(p, cnt);
    return p->sem != NULL;
}

uint8_t
gsm_sys_sem_delete(gsm_sys_sem_t* p) {
    sys_sem_free(p);
    return 0;
}

uint32_t
gsm_sys_sem_wait(gsm_sys_sem_t* p, uint32_t timeout) {
    uint32_t tick = sys_now();          /* Get start tick time */
    return (sys_arch_sem_wait((sys_sem_t*) p, !timeout ? osWaitForever : timeout) == 1) ?
        (sys_now() - tick) : GSM_SYS_TIMEOUT;    /* Wait for semaphore with specific time */
}

uint8_t
gsm_sys_sem_release(gsm_sys_sem_t* p) {
    sys_sem_signal(p);
    return 0; 
}

uint8_t
gsm_sys_sem_isvalid(gsm_sys_sem_t* p) {
    return p != NULL;                          /* Check if valid */
}

uint8_t
gsm_sys_sem_invalid(gsm_sys_sem_t* p) {
    p = GSM_SYS_SEM_NULL;                      /* Invaldiate semaphore */
    return 1;
}

uint8_t
gsm_sys_mbox_create(gsm_sys_mbox_t* b, size_t size) {
    sys_mbox_new(b, size);
    return b != NULL;
}

uint8_t
gsm_sys_mbox_delete(gsm_sys_mbox_t* b) {
    sys_mbox_free(b);
    return b != NULL;

}

uint32_t
gsm_sys_mbox_put(gsm_sys_mbox_t* b, void* m) {
    uint32_t tick =  sys_now();               /* Get start time */
    return (sys_mbox_trypost(b, m) == 0) ?
        (sys_now() - tick) : GSM_SYS_TIMEOUT; /* Put new message with forever timeout */
}

uint32_t
gsm_sys_mbox_get(gsm_sys_mbox_t* b, void* m, uint32_t timeout) {
    uint32_t time =  sys_now();          /* Get current time */
    uint32_t ret = sys_arch_mbox_fetch(b, (void**)m, timeout);
    if (ret == 1) {
        return sys_now() - time;        /* Return time required for reading message */
    }
    return GSM_SYS_TIMEOUT;
}

uint8_t
gsm_sys_mbox_putnow(gsm_sys_mbox_t* b, void* m) {
    return sys_mbox_trypost(b, m);
}

uint8_t
gsm_sys_mbox_getnow(gsm_sys_mbox_t* b, void** m) {
    uint32_t ret = 0;
    ret = sys_arch_mbox_tryfetch(b, (void**)m);
    if (ret == 0) {
        return 1;
    }
    return 0;
}

uint8_t
gsm_sys_mbox_isvalid(gsm_sys_mbox_t* b) {
    return b != NULL;                          /* Return status if message box is valid */
}

uint8_t
gsm_sys_mbox_invalid(gsm_sys_mbox_t* b) {
    b = GSM_SYS_MBOX_NULL;                     /* Invalidate message box */
    return 1;
}

uint8_t
gsm_sys_thread_create(gsm_sys_thread_t* t, const char* name, gsm_sys_thread_fn thread_func,
     void* const arg, size_t stack_size, gsm_sys_thread_prio_t prio) {

    gsm_sys_thread_t thread;
    thread = sys_thread_new(name, (lwip_thread_fn) thread_func, arg, stack_size, (int)prio);

    if (thread.thread_handle != NULL) {
        t->thread_handle = thread.thread_handle;
    }
    
    return thread.thread_handle != NULL;
}

uint8_t
gsm_sys_thread_terminate(gsm_sys_thread_t* t) {
    /* Terminate thread */
    vTaskDelete(t != NULL ? t->thread_handle : NULL);

    return 1;
}

uint8_t
gsm_sys_thread_yield(void) {
    //osThreadYield();                            /* Yield current thread */
    return 1;
}

#endif /* !__DOXYGEN__ */
