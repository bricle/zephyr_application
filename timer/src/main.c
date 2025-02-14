#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
struct k_work my_work;

void work_handler(struct k_work* work) {
    LOG_INF("timer expiried and work is excueed!");
}

K_WORK_DEFINE(my_work, work_handler);
struct k_timer timer;

void timer_cb(struct k_timer* timer_id) {
    if (timer_id == &timer) {
        LOG_INF("Timer fired!");
        k_work_submit(&my_work);
        // printk("Timer fired!\n");
    }
}

int main(void) {
    k_timer_init(&timer, timer_cb, NULL);
    // k_timer_start(&timer, K_MSEC(1000), K_MSEC(3000));

    /* example of a one shot timer with a delay of 6 seconds *
    * */
    k_timer_start(&timer, K_MSEC(6000), K_MSEC(2000));

    LOG_INF("timer started!");
    return 0;
}