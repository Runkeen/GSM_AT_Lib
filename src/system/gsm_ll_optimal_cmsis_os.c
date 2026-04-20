/**
 * \file            gsm_ll_optimal.c
 * \brief           Generic driver, included in various driver variants
 */

/*
 * How it works
 *
 * On first call to \ref gsm_ll_init, new thread is created and processed in usart_ll_thread function.
 * USART is configured in RX DMA mode and any incoming bytes are processed inside thread function.
 * DMA and USART implement interrupt handlers to notify main thread about new data ready to send to upper layer.
 *
 * \ref GSM_CFG_INPUT_USE_PROCESS must be enabled in `gsm_config.h` to use this driver.
 */
#include "gsm/gsm.h"
#include "gsm/gsm_mem.h"
#include "gsm/gsm_input.h"
#include "system/gsm_ll.h"

/* ucore includes */
#include "log_debug.h"
#include <ucore/pinmux.h>
#include <ucore/irq.h>
#include <ucore/gpio.h>
#include "usart.h"

#if !__DOXYGEN__

#if !GSM_CFG_INPUT_USE_PROCESS
#error "GSM_CFG_INPUT_USE_PROCESS must be enabled in `gsm_config.h` to use this driver."
#endif /* GSM_CFG_INPUT_USE_PROCESS */

#if !defined(GSM_USART_DMA_RX_BUFF_SIZE)
#define GSM_USART_DMA_RX_BUFF_SIZE      0x1000
#endif /* !defined(GSM_USART_DMA_RX_BUFF_SIZE) */

#if !defined(GSM_MEM_SIZE)
#define GSM_MEM_SIZE                    0x1000
#endif /* !defined(GSM_MEM_SIZE) */

// Board connection diagram
    //      MCU          GSM CHIP 
    // UART_Tx  PD2  - UART_Rx white - yellow
    // UART_Rx  PD3  - UART_Tx  grey - green
    // GND           - GND
    // 3.3V          - 3.3V

#define PIN_GSM_UART_TX  PD2
#define PIN_GSM_UART_RX  PD3

#define USART_BT_RXFIFO_INT_COUNT 0x0U // Rx BT word count
#define USART_BT_TXFIFO_INT_COUNT 0x0U // Tx BT word count

static struct usart_cfg* gsm_usart_cfg;

/* USART memory */
static uint8_t      usart_mem[GSM_USART_DMA_RX_BUFF_SIZE];
static uint8_t      is_running, initialized;
//static size_t       old_pos;

static uint16_t read_offset = 0;
static uint16_t buff_offset = 0;

/* USART thread */
#if (osCMSIS < 0x20000U)
static void usart_ll_thread(void * arg);
static osThreadDef(usart_ll_thread, usart_ll_thread, osPriorityNormal, 0, 1024);
#endif
static osThreadId usart_ll_thread_id;

/* Message queue */
#if (osCMSIS < 0x20000U)
static osMessageQDef(usart_ll_mbox, 10, uint8_t);
static osMessageQId usart_ll_mbox_id;
osMessageQDef(usart_ll_mbox, 10, uint8_t);
#else
#define USART_QUEUE_OBJECTS 16

typedef struct {                       // queue object data type
    uint8_t Buf[64];
} usart_ll_msg_queue_obj_t;

osMessageQueueAttr_t usart_ll_queue_attr;
osMessageQueueId_t usart_ll_queue_id;
usart_ll_msg_queue_obj_t usart_ll_msg;
#endif

/**
 * \brief           USART data processing
 */
static void
usart_ll_thread(void* arg) {
    GSM_UNUSED(arg);

    while (1) {
#if (osCMSIS < 0x20000U)
        osEvent evt;
        /* Wait for the event message from DMA or USART */
        evt = osMessageGet(usart_ll_mbox_id, osWaitForever);
        if (evt.status != osEventMessage) {
            continue;
        }
#else
        osStatus_t stat;
        stat = osMessageQueueGet(usart_ll_queue_id, &usart_ll_msg, NULL, 0);
        if (stat != osOK) {
            continue;
        }
#endif
        if (read_offset != buff_offset && is_running) {
            uint32_t len = buff_offset - read_offset;
            //log_buf("input", usart_mem, len);
            gsm_input_process(&usart_mem[read_offset], len);
            read_offset = read_offset + len;
        }
    }
}

__attribute__((weak)) void usart1_isr(int irq, void *priv);

/* usart for GSM device */
void usart1_init(uint32_t baudrate)
{
    /* Configure USART pins */
    ucore_pin_configure(UCORE_BUF_INPUT_ENABLE, PIN_GSM_UART_RX);      // UART_Rx pin
    ucore_pin_configure(UCORE_BUF_OUTPUT_ENABLE, PIN_GSM_UART_TX);     // UART_Tx pin

    ucore_pin_select_function(UCORE_PIN_MODE_USART1D_RXD, PIN_GSM_UART_RX);
    ucore_pin_select_function(UCORE_PIN_MODE_USART1D_TXD, PIN_GSM_UART_TX);

    /* Enable USART interrupts */
    static struct usart_cfg cfg;
    const struct ucore_device_t *udev = ucore_platform_find_device("USART_D1");
    ucore_irq_set_handler(NULL, udev->interrupts[0], 0, usart1_isr, (void *)&cfg);
       
    // interrupt priority must be higher than priority configMAX_SYSCALL_INTERRUPT_PRIORITY=128
    // port.c line 2132 configASSERT( ucCurrentPriority >= ucMaxSysCallPriority )
    // need to apply shift (8 - __NVIC_PRIO_BITS) in core_cm33.h line 2915
    // 59 - USART_D1 IRQ (exception 75)
    // ( 4 << (8 - 3) = 0x80 ) <= 128
    NVIC_SetPriority(59, 4); 

    // ucore_irq_priority_set function dont set priority interrupt,
    // because ctl->ctrl->adjust_priority not exist
    // ucore_irq_priority_set(udev->interrupts[0], osPriorityISR);

	ucore_irq_enable(udev->interrupts[0]);

    uint32_t core_freq = ucore_clk_get_frequency(UCORE_CLK_SYSCLK, NULL);

	cfg.base_addr = (void *)(udev->registers[0]);
	cfg.core_freq = core_freq;
    cfg.baud = baudrate;
    cfg.timeout = 0;
    cfg.sync_mode = 0;
    cfg.mode = 0;
    cfg.msg_len = 8;
    cfg.rx_break_len = USART_RX_BREAK_11;
    cfg.tx_break_len = USART_RX_BREAK_11;

    usart_init(&cfg, true);
    usart_configure_fifo_thresholds(&cfg, USART_BT_TXFIFO_INT_COUNT, USART_BT_RXFIFO_INT_COUNT);
    usart_irq_ctrl(&cfg, USART_IRQ_RXIM | USART_IRQ_TXIM, true);

    gsm_usart_cfg = &cfg;
}

/**
 * \brief           Configure UART 
 * */
static void
configure_uart(uint32_t baudrate) {

    if (!initialized) {

        usart1_init(baudrate);
        
#if defined(GSM_RESET_PIN)
        /* Configure RESET pin */
        gpio_reset_pin = GSM_RESET_PIN;
#endif /* defined(GSM_RESET_PIN) */

        is_running = 1;
    } else {
        osDelay(10);
        usart1_init(baudrate);
    }
    buff_offset = 0;
    read_offset = 0;

    /* Create mbox and start thread */
    if (usart_ll_queue_id == NULL) {
#if (osCMSIS < 0x20000U)
        usart_ll_queue_id = osMessageCreate(osMessageQ(usart_ll_mbox), NULL);
#else
        usart_ll_queue_id = osMessageQueueNew(USART_QUEUE_OBJECTS, 
            sizeof(usart_ll_msg_queue_obj_t), &usart_ll_queue_attr);
#endif
    }
    if (usart_ll_thread_id == NULL) {
#if (osCMSIS < 0x20000U)
        usart_ll_thread_id = osThreadCreate(osThread(usart_ll_thread), usart_ll_queue_id);
#else
        if (!gsm_sys_thread_create(&usart_ll_thread_id, "usart_task", 
            usart_ll_thread, NULL, GSM_SYS_THREAD_SS, GSM_SYS_THREAD_PRIO)) {
        GSM_DBG("%s Cannot allocate usart read thread!\n", __func__);
    }
#endif
    }
}

#if defined(GSM_RESET_PIN)
/**
 * \brief           Hardware reset callback
 */
static uint8_t
reset_device(uint8_t state) {
    if (state) {                                /* Activate reset line */
        //Reset Reset_Pin
    } else {
        //Set reset_Pin
    }
    return 1;
}
#endif /* defined(GSM_RESET_PIN) */

/**
 * \brief           Send data to GSM device
 * \param[in]       data: Pointer to data to send
 * \param[in]       len: Number of bytes to send
 * \return          Number of bytes sent
 */
static size_t
send_data(const void* data, size_t len) {

    usart_send(gsm_usart_cfg, (void*)data, len);

    return len;
}

/**
 * \brief           Callback function called from initialization process
 * \note            This function may be called multiple times if AT baudrate is changed from application
 * \param[in,out]   ll: Pointer to \ref gsm_ll_t structure to fill data for communication functions
 * \param[in]       baudrate: Baudrate to use on AT port
 * \return          Member of \ref gsmr_t enumeration
 */
gsmr_t
gsm_ll_init(gsm_ll_t* ll) {
    static uint8_t memory[GSM_MEM_SIZE];
    gsm_mem_region_t mem_regions[] = {
        { memory, sizeof(memory) }
    };

    if (!initialized) {
        ll->send_fn = send_data;                /* Set callback function to send data */
#if defined(GSM_RESET_PIN)
        ll->reset_fn = reset_device;            /* Set callback for hardware reset */
#endif /* defined(GSM_RESET_PIN) */

        gsm_mem_assignmemory(mem_regions, GSM_ARRAYSIZE(mem_regions));  /* Assign memory for allocations */
    }

    configure_uart(ll->uart.baudrate);          /* Initialize UART for communication */
    initialized = 1;
    return gsmOK;
}

/**
 * \brief           Callback function to de-init low-level communication part
 * \param[in,out]   ll: Pointer to \ref gsm_ll_t structure to fill data for communication functions
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_ll_deinit(gsm_ll_t* ll) {
    if (usart_ll_queue_id != NULL) {
        osMessageQId tmp = usart_ll_queue_id;
        usart_ll_queue_id = NULL;
        osMessageQueueDelete(tmp);
    }
    if (usart_ll_thread_id != NULL) {
        osThreadId tmp = usart_ll_thread_id;
        usart_ll_thread_id = NULL;
        osThreadTerminate(tmp);
    }
    initialized = 0;
    GSM_UNUSED(ll);
    return gsmOK;
}

static void usart1_fifo_read(void) {
    static uint8_t buff_block = 0;
    size_t len;
    if ((read_offset == buff_offset)) {
        read_offset = 0;
        buff_offset = 0;
    }
    if (buff_block) {
        if (buff_offset + 0x20 < GSM_USART_DMA_RX_BUFF_SIZE) {
            buff_block = 0;
        }
        return;
    }
    /* save rx data to buffer */
    len = usart_read(gsm_usart_cfg, (void*) usart_mem + buff_offset, GSM_USART_DMA_RX_BUFF_SIZE);
    buff_offset = buff_offset + len;

    if (buff_offset + 0x20 > GSM_USART_DMA_RX_BUFF_SIZE) {
        GSM_DBG("USART buffer overflow\n");
        buff_block = 1;
    }
}

void usart1_isr(int irq, void *priv) {
    struct usart_cfg* cfg = (struct usart_cfg*) priv;
    uint32_t status = usart_read_status(cfg);
    usart_clear_irq(cfg);
    /* Tx Complete */
    if (status & USART_IRQ_TXIM) {
        return;
    }
    /* Rx Complete */
    if (status & USART_IRQ_RXIM) {
        usart1_fifo_read();
        if (usart_ll_queue_id != NULL) {
#if (osCMSIS < 0x20000U)
            osMessagePut(usart_ll_mbox_id, 0, 0);
#else
            osMessageQueuePut(usart_ll_queue_id, &usart_ll_msg, osPriorityNormal, 0U);
#endif
        }
        return;
    }
}

#endif /* !__DOXYGEN__ */
