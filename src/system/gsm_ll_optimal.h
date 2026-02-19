/**
 * \file            gsm_ll_optimal.c
 * \brief           Low-level communication with GSM device for K1879VG1T using USART
 */

#ifndef GSM_LL_OPTIMAL
#define GSM_LL_OPTIMAL

#if !__DOXYGEN__

#define GSM_TASK_NUM 6U

enum {
    STACK_TYPE_DYNAMIC,
    STACK_TYPE_STATIC,
};

#include "system/gsm_sys_cmsis_os.h"
#include <FreeRTOS.h>

struct gsm_task_stack {
    StackType_t buf[GSM_SYS_THREAD_SS];
    StaticTask_t tcb;
};

struct gsm_ctx {
    struct gsm_task_stack* stack;
    uint8_t stack_num;
    void* priv;
    uint8_t stack_type;
};

uint8_t gsm_ctx_set(struct gsm_ctx* ctx);

#endif /* !__DOXYGEN__ */

#endif /* GSM_LL_OPTIMAL */