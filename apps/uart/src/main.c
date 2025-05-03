#include <stdint.h>
#include <sys/_intsup.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "zephyr/device.h"
#include "zephyr/sys/atomic.h"
#include "zephyr/sys/atomic_types.h"
#include "zephyr/sys/printk.h"
#include "zephyr/sys/ring_buffer.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 500

#define UART30 DT_NODELABEL(uart30)

LOG_MODULE_REGISTER(uart, LOG_LEVEL_INF);

static const struct device* uart30 = DEVICE_DT_GET(UART30);

// static uint8_t rx_buf[10] = {0}; //A buffer to store incoming UART data
// static uint8_t tx_buf[]   = {"nRF Connect SDK Fundamentals Course \n\r"};
#define MSG_SIZE 32

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);
/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos      = 0;
static atomic_t times      = 0;
struct ring_buf tx_irq_rb  = {0};
struct ring_buf rx_irq_rb  = {0};
uint8_t tx_irq_rb_buf[512] = {0};
uint8_t rx_irq_rb_buf[512] = {0};
struct k_work_delayable uart_rx_complete_work;
void uart_rx_complete(struct k_work* work);
static void uart_int_cb(const struct device* dev, void* user_data);
static void uart_async_cb(const struct device* dev, struct uart_event* evt, void* user_data);
void uart_send_str_polling(const struct device* dev, char* data);
static int uart_int_transmit(const struct device* dev, const uint8_t* buf, size_t len);

int main(void) {
    uint8_t tx_buf[32] = {0};
    int ret;
    if (!device_is_ready(uart30)) {
        return 0;
    }

    uart_send_str_polling(uart30, "uart30 polling sent string test\n\r");
#ifdef CONFIG_UART_ASYNC_API
    ret = uart_callback_set(uart30, uart_cb, NULL);
    if (ret) {
        return ret;
    }
    uart_rx_enable(uart30, rx_buf, sizeof(rx_buf), 10);
    int err;
    err = uart_tx(uart30, tx_buf, sizeof(tx_buf), SYS_FOREVER_US);
    if (err) {
        return err;
    }
    LOG_INF("sent started\r\n");
#endif
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
    uart_irq_rx_disable(uart30);
    uart_irq_tx_disable(uart30);
    ret = uart_irq_callback_user_data_set(uart30, uart_int_cb, NULL);
    k_work_init_delayable(&uart_rx_complete_work, uart_rx_complete);

    uart_irq_rx_enable(uart30);
    uart_irq_tx_enable(uart30);

    ring_buf_init(&tx_irq_rb, sizeof(tx_irq_rb_buf), tx_irq_rb_buf);
    ring_buf_init(&rx_irq_rb, sizeof(rx_irq_rb_buf), rx_irq_rb_buf);
    uart_int_transmit(uart30, "uart30 int send test\n\r", sizeof("uart30 int send test\n\r"));
    uart_int_transmit(uart30, "uart30 int send test\n\r", sizeof("uart30 int send test\n\r"));
    while (k_msgq_get(&uart_msgq, tx_buf, K_FOREVER) == 0) {
        uint32_t cnt = atomic_get(&times);
        LOG_INF("...%d...times\r\n", cnt);
        uart_send_str_polling(uart30, "Echo: ");
        uart_send_str_polling(uart30, tx_buf);
        uart_send_str_polling(uart30, "\r\n");
    }

#endif
    return 0;
}

void uart_send_str_polling(const struct device* dev, char* data) {
    uint16_t len = strlen(data);
    for (int i = 0; i < len; i++) {
        uart_poll_out(dev, data[i]);
    }
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static int uart_int_transmit(const struct device* dev, const uint8_t* buf, size_t len) {
    int written = 0;
    uart_irq_tx_disable(uart30);
    written = ring_buf_put(&tx_irq_rb, buf, len);
    LOG_INF("rb written %d", written);
    uart_irq_tx_enable(uart30);
    return written;
}

// uint8_t irq_rx_buf[32] = {0};

static void uart_int_cb(const struct device* dev, void* user_data) {
    atomic_inc(&times);
    /* 移除中断处理中的日志输出以降低堆栈使用 */
    uint8_t c;

    int ret;
    uint32_t tx_size;
    uint32_t rx_size;
    uint8_t* tx_buf;
    uint8_t* rx_buf;
    if (uart_irq_update(dev) < 0) {
        LOG_INF("ira_update failed\r\n");
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        // 巨坑，这里接收不能是UINT32_MAX，否则会一直卡在这里
        // ret = uart_fifo_read(dev, irq_rx_buf, UINT32_MAX);
        rx_size = ring_buf_put_claim(&rx_irq_rb, &rx_buf, UINT32_MAX);
        ret     = uart_fifo_read(dev, rx_buf, rx_size);
        ring_buf_put_finish(&rx_irq_rb, ret);
        k_work_schedule(&uart_rx_complete_work, K_MSEC(20));
        //)
        //     while (uart_fifo_read(dev, &c, 1) == 1) {
        //         if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
        //             /* terminate string */
        //             rx_buf[rx_buf_pos] = '\0';

        //             /* if queue is full, message is silently dropped */
        //             k_msgq_put(&uart_msgq, rx_buf, K_NO_WAIT);

        //             /* reset the buffer (it was copied to the msgq) */
        //             rx_buf_pos = 0;
        //         } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
        //             rx_buf[rx_buf_pos++] = c;
        //         }
        //         /* else: characters beyond buffer size are dropped */
        //     }
    }
    if (uart_irq_tx_ready(dev)) {
        if (ring_buf_is_empty(&tx_irq_rb) == true) {
            uart_irq_tx_disable(dev);
            LOG_INF("...tx_irq_rb is empty, disabled tx...\r\n");
            return;
        }
        tx_size = ring_buf_get_claim(&tx_irq_rb, &tx_buf, UINT32_MAX);
        LOG_INF("...tx_size claimed from ring buffer: %d...\r\n", tx_size);
        ret = uart_fifo_fill(dev, tx_buf, tx_size);
        if (ret < 0) {
            LOG_INF("uart_fifo_fill failed\r\n");
            ring_buf_get_finish(&tx_irq_rb, 0);
            return;

        } else {
            LOG_INF("uart_fifo_fill, ring buffer finished %d bytes\r\n", ret);
            ring_buf_get_finish(&tx_irq_rb, (uint32_t)ret);
        }
    }

    return;
}

void uart_rx_complete(struct k_work* work) {
    LOG_INF("...uart_rx_complete...\r\n");
    // uart_irq_rx_enable(uart30);
}
#endif
static void uart_async_cb(const struct device* dev, struct uart_event* evt, void* user_data) {
    switch (evt->type) {

        case UART_TX_DONE:
            // do something
            LOG_INF("sent completed\r\n");
            break;

        case UART_TX_ABORTED:
            // do something
            break;

        case UART_RX_RDY:
            LOG_INF("uart rx ready\r\n");
            // printf("received %d bytes\r\n", evt->data.rx.len);
            // printf("offset: %d\r\n", evt->data.rx.offset);
            break;

        case UART_RX_BUF_REQUEST:
            // do something
            break;

        case UART_RX_BUF_RELEASED:
            // do something
            break;

        case UART_RX_DISABLED:
            // do something
            uart_rx_enable(uart30, rx_buf, sizeof(rx_buf), 10);
            LOG_INF("uart disabled and now is re-enabled\r\n");
            break;

        case UART_RX_STOPPED:
            // do something
            break;

        default: break;
    }
}
