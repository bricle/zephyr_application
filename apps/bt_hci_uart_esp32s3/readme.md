
注意可以esp32s3复位后再复位主控

> In our case we’re are not using hardware flow control, we only have TX and RX lines. This will work, but we need to keep it in mind when preparing the firmware.

## reference
* [The ESP32 HCI makes any Zephyr board a Bluetooth gateway - The Golioth Developer Blog](https://blog.golioth.io/the-esp32-hci-makes-any-zephyr-board-a-bluetooth-gateway/) 

* [esp-idf/examples/bluetooth/hci/controller_hci_uart_esp32c3_and_esp32s3 at master · espressif/esp-idf](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/hci/controller_hci_uart_esp32c3_and_esp32s3) 

## 坑
* CONFIG_MAIN_STACK_SIZE默认是1024，需要改为至少2048，否则上电什么输出也没有（启用printk,未启用
log的情况下）