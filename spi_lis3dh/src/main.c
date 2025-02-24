#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
// 模式00和模式11是最常用的两种SPI模式，模式00是时钟空闲时为低电平，数据采样时钟上升沿，模式11是时钟空闲时为高电平，数据采样时钟下降沿。
// 并且这两种模式读出数据一致，都可用。
#define SPIOP SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA

struct spi_dt_spec spispec = SPI_DT_SPEC_GET(DT_NODELABEL(bme280), SPIOP, 0);

static int bme_write_reg(uint8_t reg, uint8_t value) {
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

static int bme_read_reg(uint8_t reg, uint8_t* data, uint8_t size) {
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
    uint8_t data[2] = {0};
    // 重点，要读2个字节，第一个字节是dummy byte，第二个字节才是真正的数据
    bme_read_reg(0x0f, data, 2);
    LOG_INF("Chip ID: 0x%x", data[1]);
    LOG_HEXDUMP_INF(data, sizeof(data), "2regs");
    return 0;
}