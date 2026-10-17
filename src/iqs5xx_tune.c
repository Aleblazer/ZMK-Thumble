/*
 * SPDX-License-Identifier: MIT
 *
 * Extra IQS5xx (TPS65) setup the community driver doesn't do: set the
 * active and idle-touch report rates. The pad ships with 13 ms active and
 * 50 ms idle-touch; every time a resting or slow-moving finger starts moving
 * again the first reports arrive 50 ms apart, which reads as choppy motion.
 * QMK's driver sets both to 10 ms. Idle (no touch) and LP1/LP2 rates are
 * left at their defaults to keep battery drain down on the wireless half.
 *
 * Runs at APPLICATION level, after the driver's POST_KERNEL init. Writes
 * outside a comm window are fine: the IQS5xx clock-stretches until it can
 * service them.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(thumble_iqs5xx_tune, LOG_LEVEL_INF);

#define IQS5XX_REPORT_RATE_ACTIVE 0x057A
#define IQS5XX_REPORT_RATE_IDLE_TOUCH 0x057C
#define IQS5XX_END_COMM_WINDOW 0xEEEE

static const struct i2c_dt_spec trackpad = I2C_DT_SPEC_GET(DT_NODELABEL(trackpad));

static int write_reg16(uint16_t reg, uint16_t val) {
    uint8_t buf[4] = {reg >> 8, reg & 0xFF, val >> 8, val & 0xFF};

    return i2c_write_dt(&trackpad, buf, sizeof(buf));
}

static int thumble_iqs5xx_tune_init(void) {
    const uint16_t rate = CONFIG_THUMBLE_IQS5XX_REPORT_RATE_MS;
    int ret = -ENODEV;

    if (!i2c_is_ready_dt(&trackpad)) {
        LOG_ERR("I2C bus not ready");
        return 0;
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        ret = write_reg16(IQS5XX_REPORT_RATE_ACTIVE, rate);
        if (ret == 0) {
            ret = write_reg16(IQS5XX_REPORT_RATE_IDLE_TOUCH, rate);
        }
        if (ret == 0) {
            break;
        }
        k_msleep(20);
    }

    uint8_t end_comm[3] = {IQS5XX_END_COMM_WINDOW >> 8, IQS5XX_END_COMM_WINDOW & 0xFF, 0x00};
    i2c_write_dt(&trackpad, end_comm, sizeof(end_comm));

    if (ret == 0) {
        LOG_INF("Report rate set to %u ms (active, idle touch)", rate);
    } else {
        LOG_ERR("Failed to set report rate: %d", ret);
    }

    /* Never fail boot over a tuning write; the pad still works on defaults. */
    return 0;
}

SYS_INIT(thumble_iqs5xx_tune_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
