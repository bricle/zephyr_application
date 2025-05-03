#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <lis3dh_driver.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

typedef enum {
    LIS3DH_STATE_STATIONARY, // Device is at rest
    LIS3DH_STATE_MOVING      // Device is in motion
} lis3dh_state_t;

#define LIS3_INT_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec lis3_int = GPIO_DT_SPEC_GET(LIS3_INT_NODE, lis3dh_int_gpios);
static struct gpio_callback lis3_int_cb;
void lis3_int_isr(const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins);
void lis3dh_work_cb(struct k_work* work);
K_WORK_DELAYABLE_DEFINE(lis3dh_work, lis3dh_work_cb);

// 模式00和模式11是最常用的两种SPI模式，模式00是时钟空闲时为低电平，数据采样时钟上升沿，模式11是时钟空闲时为高电平，数据采样时钟下降沿。
// 并且这两种模式读出数据一致，都可用。
#define SPIOP SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA
struct spi_dt_spec spispec = SPI_DT_SPEC_GET(DT_NODELABEL(lis3dh), SPIOP, 0);

static int lis3dh_write_reg(uint8_t reg, uint8_t value) {
    int err;

    uint8_t tx_buf[]                  = {(reg & 0x7F), value};
    struct spi_buf tx_spi_buf         = {.buf = tx_buf, .len = sizeof(tx_buf)};
    struct spi_buf_set tx_spi_buf_set = {.buffers = &tx_spi_buf, .count = 1};

    err = spi_write_dt(&spispec, &tx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_write_dt() failed, err %d", err);
        return err;
    }

    return 0;
}

static int lis3dh_read_reg(uint8_t reg, uint8_t* data, uint8_t size) {
    int err;

    uint8_t tx_buffer                 = reg | 0x80;
    struct spi_buf tx_spi_buf         = {.buf = (void*)&tx_buffer, .len = 1};
    struct spi_buf_set tx_spi_buf_set = {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs        = {.buf = data, .len = size};
    struct spi_buf_set rx_spi_buf_set = {.buffers = &rx_spi_bufs, .count = 1};

    err = spi_transceive_dt(&spispec, &tx_spi_buf_set, &rx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_transceive_dt() failed, err: %d", err);
        return err;
    }

    return 0;
}

int main(void) {
    int err = spi_is_ready_dt(&spispec);
    if (!err) {
        LOG_ERR("Error: SPI device is not ready, err: %d", err);
        return 0;
    }
    err = gpio_is_ready_dt(&lis3_int);
    if (!err) {
        LOG_ERR("Error: GPIO device is not ready, err: %d", err);
        return 0;
    }
    err = gpio_pin_configure_dt(&lis3_int, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Error: gpio_pin_configure_dt() failed, err: %d", err);
        return 0;
    }
    err = gpio_pin_interrupt_configure_dt(&lis3_int, GPIO_INT_EDGE_BOTH);
    if (err < 0) {
        LOG_ERR("Error: gpio_pin_interrupt_configure_dt() failed, err: %d", err);
        return 0;
    }
    gpio_init_callback(&lis3_int_cb, lis3_int_isr, BIT(lis3_int.pin));
    gpio_add_callback_dt(&lis3_int, &lis3_int_cb);
    k_work_init_delayable(&lis3dh_work, lis3dh_work_cb);
    uint8_t data[2] = {0};
    // 重点，要读2个字节，第一个字节是dummy byte，第二个字节才是真正的数据
    lis3dh_read_reg(LIS3DH_WHO_AM_I, data, 2);

    lis3dh_write_reg(LIS3DH_CTRL_REG1, 0x2f); //50HZ,低功耗模式
    lis3dh_write_reg(LIS3DH_CTRL_REG2, 0x09); //使用高通滤波器，过滤初始加速度
    lis3dh_write_reg(LIS3DH_CTRL_REG3, 0x40); //打开INT1中断
    lis3dh_write_reg(LIS3DH_CTRL_REG4, 0x00); //-2g~2g
    lis3dh_write_reg(LIS3DH_CTRL_REG5, 0x00); //锁存INT1_SRC
    lis3dh_write_reg(LIS3DH_CTRL_REG6, 0x00);

    lis3dh_write_reg(LIS3DH_INT1_THS, 0x0B);
    //设置阈值，2g单位为16mg，4g单位为32mg，8g单位为62mg，16g单位为186mg
    lis3dh_write_reg(LIS3DH_INT1_DURATION, 0x00);   //设置唤醒时间，不需要进入睡眠
    lis3dh_read_reg(LIS3DH_REFERENCE_REG, data, 2); //读取参考值寄存器，清除直流分量
    lis3dh_write_reg(LIS3DH_INT1_CFG, 0x2A);        //当XYZ轴加速度高于阈值，产生中断

    return 0;
}

void lis3_int_isr(const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins) {
    k_work_reschedule(&lis3dh_work, K_MSEC(0));
}

void lis3dh_work_cb(struct k_work* work) {
    int ret = gpio_pin_get_dt(&lis3_int);
    if (ret < 0) {
        LOG_ERR("Error: gpio_pin_get_dt() failed, err: %d", ret);
        return;
    }
    lis3dh_state_t state = ret ? LIS3DH_STATE_MOVING : LIS3DH_STATE_STATIONARY;
    switch (state) {
        case LIS3DH_STATE_STATIONARY: LOG_INF("Stationary"); break;
        case LIS3DH_STATE_MOVING:     LOG_INF("Moving"); break;
        default:                      break;
    }
}