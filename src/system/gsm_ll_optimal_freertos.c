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

#if !__DOXYGEN__

#if !GSM_CFG_INPUT_USE_PROCESS
#error "GSM_CFG_INPUT_USE_PROCESS must be enabled in `gsm_config.h` to use this driver."
#endif /* GSM_CFG_INPUT_USE_PROCESS */

#if !defined(GSM_USART_DMA_RX_BUFF_SIZE)
#define GSM_USART_DMA_RX_BUFF_SIZE      0x100
#endif /* !defined(GSM_USART_DMA_RX_BUFF_SIZE) */

#if !defined(GSM_MEM_SIZE)
#define GSM_MEM_SIZE                    0x100
#endif /* !defined(GSM_MEM_SIZE) */

// Board connection diagram
    //      MCU          GSM CHIP 
    // UART_Tx  PD2  - UART_Rx white 
    // UART_Rx  PD3  - UART_Tx grey 
    // GND           - GND
    // 3.3V          - 3.3V 

#define PIN_GSM_UART_TX  PD2
#define PIN_GSM_UART_RX  PD3

#define USART_GSM_RXFIFO_INT_COUNT 0x0U // Rx BT word count
#define USART_GSM_TXFIFO_INT_COUNT 0x0U // Tx BT word count

static struct usart_cfg* gsm_usart_cfg;

/* USART memory */
static uint8_t      usart_mem[GSM_USART_DMA_RX_BUFF_SIZE];
static uint8_t      initialized;

static uint16_t read_offset = 0;
static uint16_t buff_offset = 0;

/**
 * \brief           USART data processing
 */
static void
usart_ll_thread(void* arg) {
    gsm_ll_t* ll = (gsm_ll_t*) arg;

    /* Thread is running, unlock semaphore */
    if (gsm_sys_sem_isvalid(ll->sem)) {
        gsm_sys_sem_release(ll->sem);      /* Release semaphore */
    }

    while (1) {
        if (!gsm_sys_mbox_getnow(&ll->mbox, (void*) &ll->message)) {
            continue;
        }

        if (read_offset != buff_offset) {
            uint32_t len = buff_offset - read_offset;
            gsm_input_process(&usart_mem[read_offset], len);
            read_offset = read_offset + len;
        }
    }
}

__attribute__((weak)) void usart_ll_isr(int irq, void *priv);

/* usart for GSM device */
struct usart_cfg* usart_ll_init(gsm_ll_t* ll)
{
    /* Configure USART pins */
    ucore_pin_configure(UCORE_BUF_INPUT_ENABLE, PIN_GSM_UART_RX);      // UART_Rx pin
    ucore_pin_configure(UCORE_BUF_OUTPUT_ENABLE, PIN_GSM_UART_TX);     // UART_Tx pin

    ucore_pin_select_function(UCORE_PIN_MODE_USART1D_RXD, PIN_GSM_UART_RX);
    ucore_pin_select_function(UCORE_PIN_MODE_USART1D_TXD, PIN_GSM_UART_TX);

    /* Enable USART interrupts */
    static struct usart_cfg cfg;
    const struct ucore_device_t *udev = ucore_platform_find_device("USART_D1");
    ucore_irq_set_handler(NULL, udev->interrupts[0], 0, usart_ll_isr, (void *)ll);
       
    // interrupt priority must be higher than priority configMAX_SYSCALL_INTERRUPT_PRIORITY=128
    // port.c line 2132 configASSERT( ucCurrentPriority >= ucMaxSysCallPriority )
    // need to apply shift (8 - __NVIC_PRIO_BITS) in core_cm33.h line 2915
    // 59 - USART_D1 IRQ (exception 75)
    // ( 4 << (8 - 3) = 0x80 ) <= 128
    
    // NVIC_SetPriority(59, 4);

    // ucore_irq_priority_set function dont set priority interrupt,
    // because ctl->ctrl->adjust_priority not exist
    // ucore_irq_priority_set(udev->interrupts[0], osPriorityISR);

	ucore_irq_enable(udev->interrupts[0]);

    uint32_t core_freq = ucore_clk_get_frequency(UCORE_CLK_SYSCLK, NULL);

	cfg.base_addr = (void *)(udev->registers[0]);
	cfg.core_freq = core_freq;
    cfg.baud = ll->uart.baudrate;
    cfg.timeout = 0;
    cfg.sync_mode = 0;
    cfg.mode = 0;
    cfg.msg_len = 8;
    cfg.rx_break_len = USART_RX_BREAK_11;
    cfg.tx_break_len = USART_RX_BREAK_11;

    usart_init(&cfg, true);
    usart_configure_fifo_thresholds(&cfg, USART_GSM_TXFIFO_INT_COUNT, USART_GSM_RXFIFO_INT_COUNT);
    usart_irq_ctrl(&cfg, USART_IRQ_RXIM | USART_IRQ_TXIM, true);

    return &cfg;
}

/**
 * \brief           Configure UART 
 * */
static void
configure_uart(gsm_ll_t* ll) {

    if (!initialized) {
        gsm_usart_cfg = (void*)usart_ll_init(ll);
#if defined(GSM_RESET_PIN)
        /* Configure RESET pin */
        gpio_reset_pin = GSM_RESET_PIN;
#endif /* defined(GSM_RESET_PIN) */

    } else {
        gsm_delay(10);
        gsm_usart_cfg = (void*)usart_ll_init(ll);
    }
    
    ll->uart.cfg = gsm_usart_cfg;
    buff_offset = 0;
    read_offset = 0;

    /* Create mbox and start thread */
    if (ll->mbox.mbx == NULL) {
        gsm_sys_mbox_create(&ll->mbox, sizeof(usart_ll_msg_t));
    }

    if (ll->thread_id.thread_handle == NULL) {
        if (!gsm_sys_thread_create(&ll->thread_id, "usart_task", usart_ll_thread, 
            ll, GSM_SYS_THREAD_SS, GSM_SYS_THREAD_PRIO)) {
        GSM_DBG("%s Cannot allocate usart read thread!\n", __func__);
        }
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

    configure_uart(ll);          /* Initialize UART for communication */
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
    GSM_UNUSED(ll);

    if (ll->mbox.mbx != NULL) {
        gsm_sys_mbox_t* tmp = &ll->mbox;
        ll->mbox.mbx = NULL;
        gsm_sys_mbox_delete(tmp);
    }
    if (ll->thread_id.thread_handle != NULL) {
        gsm_sys_thread_t* tmp = &ll->thread_id;
        ll->thread_id.thread_handle = NULL;
        gsm_sys_thread_terminate(tmp);
    }
    initialized = 0;
    
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

void usart_ll_isr(int irq, void *priv) {
    gsm_ll_t* ll = (gsm_ll_t*) priv;
    struct usart_cfg* cfg = (struct usart_cfg*) ll->uart.cfg;
    uint32_t status = usart_read_status(cfg);
    usart_clear_irq(cfg);
    /* Tx Complete */
    if (status & USART_IRQ_TXIM) {
        return;
    }
    /* Rx Complete */
    if (status & USART_IRQ_RXIM) {
        usart1_fifo_read();
        if (ll->mbox.mbx != NULL) {
            gsm_sys_mbox_put(&ll->mbox, &ll->message);
        }
        return;
    }
}

#endif /* !__DOXYGEN__ */
