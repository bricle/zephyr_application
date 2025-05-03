#include <stdint.h>
#include <sys/_intsup.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include "zephyr/sys/util_macro.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define DELAY_REG    10
#define DELAY_PARAM  50
#define DELAY_VALUES 1000
#define LED0         13

#define CTRLHUM   0xF2
#define CTRLMEAS  0xF4
#define CALIB00   0x88
#define CALIB26   0xE1
#define ID        0xD0
#define PRESSMSB  0xF7
#define PRESSLSB  0xF8
#define PRESSXLSB 0xF9
#define TEMPMSB   0xFA
#define TEMPLSB   0xFB
#define TEMPXLSB  0xFC
#define HUMMSB    0xFD
#define HUMLSB    0xFE
#define DUMMY     0xFF

struct bme280_data {
    /* Compensation parameters */
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;

    /* Compensated values */
    int32_t comp_temp;
    uint32_t comp_press;
    uint32_t comp_humidity;

    /* Carryover between temperature and pressure/humidity compensation */
    int32_t t_fine;

    uint8_t chip_id;
} bmedata;

int bme_read_sample(void);
void bme_calibrationdata(void);

struct spi_config bme280_config = {0};
#define spiop       SPI_TRANSFER_MSB | SPI_WORD_SET(8)
#define spi_node_id DT_NODELABEL(bme280)
struct spi_dt_spec bem280_device = SPI_DT_SPEC_GET(spi_node_id, spiop, 0);

static int bme_read_reg(uint8_t reg, uint8_t* data, uint8_t size);

int main(void) {

    int ret;
    ret = spi_is_ready_dt(&bem280_device);
    if (ret < 0) {
        LOG_ERR("SPI device not ready");
        return ret;
    }
    /****************************************wrong command patten ********************/
    /** write api */
    uint8_t tx_buf[1]         = {0xD0 | BIT(7)};
    struct spi_buf test_w_buf = {
        .buf = &tx_buf[0],
        .len = sizeof(tx_buf),
    };
    struct spi_buf_set test_w_buf_set = {
        .buffers = &test_w_buf,
        .count   = 1,
    };
    ret = spi_write_dt(&bem280_device, &test_w_buf_set);
    if (ret < 0) {
        LOG_ERR("SPI write failed");
        return ret;
    }

    /** read api */
    uint8_t rx_buf[2]         = {0x00, 0x00};
    struct spi_buf test_r_buf = {
        .buf = &rx_buf[0],
        .len = sizeof(rx_buf),
    };
    struct spi_buf_set test_r_buf_set = {
        .buffers = &test_r_buf,
        .count   = 1,
    };
    ret = spi_read_dt(&bem280_device, &test_r_buf_set);
    if (ret < 0) {
        LOG_ERR("SPI write failed");
        return ret;
    }
    LOG_INF("SPI read success, data: %x %x", rx_buf[0], rx_buf[1]);
    /****************************************right  command patten ********************/
    uint8_t transceive_tx_buf[1] = {0xD0 | BIT(7)};
    struct spi_buf trans_tx_buf  = {
         .buf = &transceive_tx_buf[0],
         .len = sizeof(transceive_tx_buf),
    };
    struct spi_buf_set transceive_tx_buf_set = {
        .buffers = &trans_tx_buf,
        .count   = 1,
    };

    uint8_t transceive_rx_buf[2] = {0};
    struct spi_buf trans_rx_buf  = {
         .buf = &transceive_rx_buf[0],
         .len = sizeof(transceive_rx_buf),
    };
    struct spi_buf_set transceive_rx_buf_set = {
        .buffers = &trans_rx_buf,
        .count   = 1,
    };
    ret = spi_transceive_dt(&bem280_device, &transceive_tx_buf_set, &transceive_rx_buf_set);
    if (ret < 0) {
        LOG_ERR("SPI transceive failed");
        return ret;
    }
    LOG_INF("SPI transceive success, data: %x %x", transceive_rx_buf[0], transceive_rx_buf[1]);

    /*****************************right command patten, but wrapped ********************/
    uint8_t id[2] = {0};
    ret           = bme_read_reg(0xD0, &id[0], sizeof(id));
    if (ret < 0) {
        LOG_ERR(" read failed");
        return ret;
    }
    LOG_INF("BME280 ID using : %x %x", id[0], id[1]);
    bme_calibrationdata();

    bme_read_sample();
    return 0;
}

static int bme_read_reg(uint8_t reg, uint8_t* data, uint8_t size) {
    int err;

    /* STEP 4.1 - Set the transmit and receive buffers */
    uint8_t t_buf[1]      = {reg | BIT(7)};
    struct spi_buf tx_buf = {
        .buf = &t_buf[0],
        .len = sizeof(t_buf),
    };
    struct spi_buf_set tx_buf_set = {
        .buffers = &tx_buf,
        .count   = 1,
    };
    struct spi_buf rx_buf = {
        .buf = data,
        .len = size,
    };
    struct spi_buf_set rx_buf_set = {
        .buffers = &rx_buf,
        .count   = 1,
    };
    /* STEP 4.2 - Call the transceive function */
    err = spi_transceive_dt(&bem280_device, &tx_buf_set, &rx_buf_set);
    if (err < 0) {
        LOG_ERR("SPI transceive failed");
        return err;
    }
    // LOG_INF("SPI transceive success, data: %x %x", transceive_rx_buf[0], transceive_rx_buf[1]);
    return 0;
}

static int bme_write_reg(uint8_t reg, uint8_t value) {
    int err;

    /* STEP 5.1 - delcare a tx buffer having register address and data */
    uint8_t t_buf[2]      = {reg & 0x7F, value};
    struct spi_buf tx_buf = {
        .buf = &t_buf[0],
        .len = sizeof(t_buf),
    };
    struct spi_buf_set t_buf_set = {
        .buffers = &tx_buf,
        .count   = 1,
    };
    err = spi_write_dt(&bem280_device, &t_buf_set);
    if (err < 0) {
        LOG_ERR("SPI write failed");
        return err;
    }
    /* STEP 5.2 - call the spi_write_dt function with SPISPEC to write buffers */

    return 0;
}

/* STEP 8 - Go through the compensation functions and 
  note that they use the compensation parameters from 
  the bme280_data structure and then store back the compensated value */
static void bme280_compensate_temp(struct bme280_data* data, int32_t adc_temp) {
    int32_t var1, var2;

    var1 = (((adc_temp >> 3) - ((int32_t)data->dig_t1 << 1)) * ((int32_t)data->dig_t2)) >> 11;
    var2 = (((((adc_temp >> 4) - ((int32_t)data->dig_t1))
              * ((adc_temp >> 4) - ((int32_t)data->dig_t1)))
             >> 12)
            * ((int32_t)data->dig_t3))
           >> 14;

    data->t_fine    = var1 + var2;
    data->comp_temp = (data->t_fine * 5 + 128) >> 8;
}

static void bme280_compensate_press(struct bme280_data* data, int32_t adc_press) {
    int64_t var1, var2, p;

    var1 = ((int64_t)data->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)data->dig_p6;
    var2 = var2 + ((var1 * (int64_t)data->dig_p5) << 17);
    var2 = var2 + (((int64_t)data->dig_p4) << 35);
    var1 = ((var1 * var1 * (int64_t)data->dig_p3) >> 8) + ((var1 * (int64_t)data->dig_p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)data->dig_p1) >> 33;

    /* Avoid exception caused by division by zero */
    if (var1 == 0) {
        data->comp_press = 0U;
        return;
    }

    p    = 1048576 - adc_press;
    p    = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)data->dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)data->dig_p8) * p) >> 19;
    p    = ((p + var1 + var2) >> 8) + (((int64_t)data->dig_p7) << 4);

    data->comp_press = (uint32_t)p;
}

static void bme280_compensate_humidity(struct bme280_data* data, int32_t adc_humidity) {
    int32_t h;

    h = (data->t_fine - ((int32_t)76800));
    h = ((((adc_humidity << 14) - (((int32_t)data->dig_h4) << 20) - (((int32_t)data->dig_h5) * h))
          + ((int32_t)16384))
         >> 15)
        * (((((((h * ((int32_t)data->dig_h6)) >> 10)
               * (((h * ((int32_t)data->dig_h3)) >> 11) + ((int32_t)32768)))
              >> 10)
             + ((int32_t)2097152))
                * ((int32_t)data->dig_h2)
            + 8192)
           >> 14);
    h = (h - (((((h >> 15) * (h >> 15)) >> 7) * ((int32_t)data->dig_h1)) >> 4));
    h = (h > 419430400 ? 419430400 : h);

    data->comp_humidity = (uint32_t)(h >> 12);
}

int bme_read_sample(void) {

    int err;

    int32_t datap = 0, datat = 0, datah = 0;
    float pressure = 0.0f, temperature = 0.0f, humidity = 0.0f;

    /* STEP 9.1 - Store register addresses to do burst read */
    // uint8_t regs[] = {PRESSMSB,
    //                   PRESSLSB,
    //                   PRESSXLSB,
    //                   TEMPMSB,
    //                   TEMPLSB,
    //                   TEMPXLSB,
    //                   HUMMSB,
    //                   HUMLSB,
    //                   DUMMY}; //0xFF is dummy reg
    // uint8_t readbuf[sizeof(regs)];
    uint8_t regs[] = {PRESSMSB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //0xFF is dummy reg
    uint8_t readbuf[sizeof(regs)];
    /* STEP 9.2 - Set the transmit and receive buffers */
    /* STEP 9.2 - Set the transmit and receive buffers */
    struct spi_buf tx_spi_buf         = {.buf = (void*)&regs, .len = sizeof(regs)};
    struct spi_buf_set tx_spi_buf_set = {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_bufs        = {.buf = readbuf, .len = sizeof(regs)};
    struct spi_buf_set rx_spi_buf_set = {.buffers = &rx_spi_bufs, .count = 1};

    /* STEP 9.3 - Use spi_transceive() to transmit and receive at the same time */
    err = spi_transceive_dt(&bem280_device, &tx_spi_buf_set, &rx_spi_buf_set);
    if (err < 0) {
        LOG_ERR("spi_transceive_dt() failed, err: %d", err);
        return err;
    }
    LOG_HEXDUMP_INF(readbuf, sizeof(readbuf), "read data:");
    /* STEP 9.3 - Use spi_transceive() to transmit and receive at the same time */

    /* Put the data read from registers into actual order (see datasheet) */
    /* Uncompensated pressure value */
    datap = (readbuf[1] << 12) | (readbuf[2] << 4) | ((readbuf[3] >> 4) & 0x0F);
    /* Uncompensated temperature value */
    datat = (readbuf[4] << 12) | (readbuf[5] << 4) | ((readbuf[6] >> 4) & 0x0F);
    /* Uncompensated humidity value */
    datah = (readbuf[7] << 8) | (readbuf[8]);

    /* Compensate values using given functions */
    bme280_compensate_press(&bmedata, datap);
    bme280_compensate_temp(&bmedata, datat);
    bme280_compensate_humidity(&bmedata, datah);

    /* Convert to proper format as per data sheet */
    pressure    = (float)bmedata.comp_press / 256.0;
    temperature = (float)bmedata.comp_temp / 100.0;
    humidity    = (float)bmedata.comp_humidity / 1024.0;

    /* Print the uncompensated and compensated values */
    LOG_INF("\tTemperature: \t uncomp = %d C \t comp = %8.2f C", datat, (double)temperature);
    LOG_INF("\tPressure:    \t uncomp = %d Pa \t comp = %8.2f Pa", datap, (double)pressure);
    LOG_INF("\tHumidity:    \t uncomp = %d RH \t comp = %8.2f %%RH", datah, (double)humidity);

    return 0;
}

void bme_calibrationdata(void) {
    /* Set data size of 3 as the first byte is dummy using bme_read_reg() */
    uint8_t values[3];
    uint8_t size = 3;

    uint8_t regaddr;
    LOG_INF("-------------------------------------------------------------");
    LOG_INF("bme_read_calibrationdata: Reading from calibration registers:");
    /* STEP 6 - We are using bme_read_reg() to read required number of bytes from 
	respective register(s) and put values to construct compensation parameters */
    regaddr = CALIB00;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_t1 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param T1 = %d", regaddr, size - 1, bmedata.dig_t1);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_t2 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param Param T2 = %d", regaddr, size - 1, bmedata.dig_t2);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_t3 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param T3 = %d", regaddr, size - 1, bmedata.dig_t3);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p1 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P1 = %d", regaddr, size - 1, bmedata.dig_p1);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p2 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P2 = %d", regaddr, size - 1, bmedata.dig_p2);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p3 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P3 = %d", regaddr, size - 1, bmedata.dig_p3);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p4 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P4 = %d", regaddr, size - 1, bmedata.dig_p4);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p5 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P5 = %d", regaddr, size - 1, bmedata.dig_p5);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p6 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P6 = %d", regaddr, size - 1, bmedata.dig_p6);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p7 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P7 = %d", regaddr, size - 1, bmedata.dig_p7);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p8 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P8 = %d", regaddr, size - 1, bmedata.dig_p8);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_p9 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param P9 = %d", regaddr, size - 1, bmedata.dig_p9);
    k_msleep(DELAY_PARAM);

    regaddr += 3;
    size = 2; /* only read one byte for H1 (see datasheet) */
    bme_read_reg(regaddr, values, size);
    bmedata.dig_h1 = (uint8_t)values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param H1 = %d", regaddr, size - 1, bmedata.dig_h1);
    k_msleep(DELAY_PARAM);

    regaddr += 64;
    size = 3; /* read two bytes for H2 */
    bme_read_reg(regaddr, values, size);
    bmedata.dig_h2 = ((uint16_t)values[2]) << 8 | values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param H2 = %d", regaddr, size - 1, bmedata.dig_h2);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    size = 2; /* only read one byte for H3 */
    bme_read_reg(regaddr, values, size);
    bmedata.dig_h3 = (uint8_t)values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param H3 = %d", regaddr, size - 1, bmedata.dig_h3);
    k_msleep(DELAY_PARAM);

    regaddr += 1;
    size = 3; /* read two bytes for H4 */
    bme_read_reg(regaddr, values, size);
    bmedata.dig_h4 = ((uint16_t)values[1]) << 4 | (values[2] & 0x0F);
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param H4 = %d", regaddr, size - 1, bmedata.dig_h4);
    k_msleep(DELAY_PARAM);

    regaddr += 1;
    bme_read_reg(regaddr, values, size);
    bmedata.dig_h5 = ((uint16_t)values[2]) << 4 | ((values[1] >> 4) & 0x0F);
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param H5 = %d", regaddr, size - 1, bmedata.dig_h5);
    k_msleep(DELAY_PARAM);

    regaddr += 2;
    size = 2; /* only read one byte for H6 */
    bme_read_reg(regaddr, values, 2);
    bmedata.dig_h6 = (uint8_t)values[1];
    LOG_INF("\tReg[0x%02x] %d Bytes read: Param H6 = %d", regaddr, size - 1, bmedata.dig_h6);
    k_msleep(DELAY_PARAM);
    LOG_INF("-------------------------------------------------------------");
}