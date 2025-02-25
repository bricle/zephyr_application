/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <sys/_intsup.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "zephyr/device.h"
#include "zephyr/sys/atomic.h"
#include "zephyr/sys/atomic_types.h"
#include "zephyr/sys/printk.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 500

#define UART30 DT_NODELABEL(uart30)
// #define BUTTON0_NODE DT_NODELABEL(button0)

LOG_MODULE_REGISTER(uart, LOG_LEVEL_INF);

static const struct device* uart30 = DEVICE_DT_GET(UART30);

// static uint8_t rx_buf[10] = {0}; //A buffer to store incoming UART data
// static uint8_t tx_buf[]   = {"nRF Connect SDK Fundamentals Course \n\r"};
#define MSG_SIZE 32

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);
/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;
static atomic_t times = 0;
static void uart_int_cb(const struct device* dev, void* user_data);
static void uart_async_cb(const struct device* dev, struct uart_event* evt, void* user_data);
void uart_send_str_int(const struct device* dev, char* data);

int main(void) {
    char tx_buf[MSG_SIZE];
    int ret;
    if (!device_is_ready(uart30)) {
        return 0;
    }

    // uart_poll_out(uart30, 'd');
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
    ret = uart_irq_callback_user_data_set(uart30, uart_int_cb, NULL);

    if (ret < 0) {
        if (ret == -ENOTSUP) {
            printk("Interrupt-driven UART API support not enabled\n");
        } else if (ret == -ENOSYS) {
            printk("UART device does not support interrupt-driven API\n");
        } else {
            printk("Error setting UART callback: %d\n", ret);
        }
        return 0;
    }
    uart_irq_rx_enable(uart30);
    uart_send_str_int(uart30, "uart30 echo test\n\r");
    while (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == 0) {
        uint32_t cnt = atomic_get(&times);
        LOG_INF("...%d...times\r\n", cnt);
        uart_send_str_int(uart30, "Echo: ");
        uart_send_str_int(uart30, tx_buf);
        uart_send_str_int(uart30, "\r\n");
    }
#endif
    // uint8_t read_uart;
    // do {
    //     ret = uart_poll_in(uart30, &read_uart);
    //     k_msleep(1);
    // } while (ret == -1);
    // if (ret == 0) {
    //     printk("read a data %c\r\n", read_uart);
    // } else if (ret == -1) {
    //     printk("read nothing\r\n");
    // } else if (ret == -ENOSYS) {
    //     printk("error is nosys\r\n");
    // } else if (ret == -EBUSY) {
    //     printk("error busy\r\n");
    // } else {
    //     printk("not all above\r\n");
    // }

    return 0;
}

void uart_send_str_int(const struct device* dev, char* data) {
    uint16_t len = strlen(data);
    for (int i = 0; i < len; i++) {
        uart_poll_out(dev, data[i]);
    }
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static void uart_int_cb(const struct device* dev, void* user_data) {
    // times++;
    atomic_inc(&times);
    uint8_t c;
    if (!uart_irq_update(dev)) {
        LOG_INF("ira_update failed\r\n");
        return;
    }
    if (!uart_irq_rx_ready(dev)) {
        LOG_INF("rx not ready\r\n");
        return;
    }
    /* read until FIFO empty */
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
            /* terminate string */
            rx_buf[rx_buf_pos] = '\0';

            /* if queue is full, message is silently dropped */
            k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

            /* reset the buffer (it was copied to the msgq) */
            rx_buf_pos = 0;
        } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
            rx_buf[rx_buf_pos++] = c;
        }
        /* else: characters beyond buffer size are dropped */
    }
    LOG_INF("...uart_int_cb...\r\n");
    return;
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
