/*
 * SPDX-License-Identifier: MIT
 *
 * Sets the TPS65's X/Y output resolution, which the community driver leaves
 * at the pad's factory value. Resolution is pad "pixels" across the full
 * width/height, so it sets how many counts a millimetre of finger travel
 * produces: higher = more sensitive and finer slow-speed steps (the driver's
 * stationary-threshold is in these pixels, so it shrinks physically too).
 * QMK's TPS65 maximum is 3072 x 2048 for the 65 x 49 mm pad.
 *
 * Runs at APPLICATION level, after the driver's POST_KERNEL init. One-time
 * register writes at boot; no extra traffic per report. Writes outside a
 * comm window are fine: the IQS5xx clock-stretches until it can service them.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(thumble_iqs5xx_resolution, LOG_LEVEL_INF);

#define IQS5XX_X_RESOLUTION 0x066E /* followed by Y resolution at 0x0670 */
#define IQS5XX_END_COMM_WINDOW 0xEEEE

static const struct i2c_dt_spec trackpad = I2C_DT_SPEC_GET(DT_NODELABEL(trackpad));

static int read_resolution(uint16_t *x, uint16_t *y) {
    uint8_t reg[2] = {IQS5XX_X_RESOLUTION >> 8, IQS5XX_X_RESOLUTION & 0xFF};
    uint8_t buf[4];
    int ret = i2c_write_read_dt(&trackpad, reg, sizeof(reg), buf, sizeof(buf));

    if (ret == 0) {
        *x = (buf[0] << 8) | buf[1];
        *y = (buf[2] << 8) | buf[3];
    }
    return ret;
}

static int write_resolution(uint16_t x, uint16_t y) {
    uint8_t buf[6] = {IQS5XX_X_RESOLUTION >> 8, IQS5XX_X_RESOLUTION & 0xFF,
                      x >> 8, x & 0xFF, y >> 8, y & 0xFF};

    return i2c_write_dt(&trackpad, buf, sizeof(buf));
}

static int thumble_iqs5xx_resolution_init(void) {
    const uint16_t want_x = CONFIG_THUMBLE_IQS5XX_RESOLUTION_X;
    const uint16_t want_y = CONFIG_THUMBLE_IQS5XX_RESOLUTION_Y;
    uint16_t old_x = 0, old_y = 0, new_x = 0, new_y = 0;
    int ret = -ENODEV;

    if (!i2c_is_ready_dt(&trackpad)) {
        LOG_ERR("I2C bus not ready");
        return 0;
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        ret = read_resolution(&old_x, &old_y);
        if (ret == 0) {
            ret = write_resolution(want_x, want_y);
        }
        if (ret == 0) {
            ret = read_resolution(&new_x, &new_y);
        }
        if (ret == 0) {
            break;
        }
        k_msleep(20);
    }

    uint8_t end_comm[3] = {IQS5XX_END_COMM_WINDOW >> 8, IQS5XX_END_COMM_WINDOW & 0xFF, 0x00};
    i2c_write_dt(&trackpad, end_comm, sizeof(end_comm));

    if (ret == 0) {
        LOG_INF("Resolution %u x %u -> %u x %u (requested %u x %u)", old_x, old_y, new_x, new_y,
                want_x, want_y);
    } else {
        LOG_ERR("Failed to set resolution: %d", ret);
    }

    /* Never fail boot over this; the pad still works at its factory resolution. */
    return 0;
}

SYS_INIT(thumble_iqs5xx_resolution_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
