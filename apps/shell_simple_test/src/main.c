#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

void main(void) {
    printk("Hello World! %s\n", CONFIG_BOARD);
    k_msleep(1000);
    printk("Hello World! %s\n", CONFIG_BOARD);
}
