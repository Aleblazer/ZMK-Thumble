/*
 * SPDX-License-Identifier: MIT
 *
 * TPS65 bring-up probe: scans the trackpad's I2C bus and reads the IQS5xx
 * identification registers, logging the results every 10 s so they show up
 * whenever the USB serial log is opened.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(thumble_i2c_probe, LOG_LEVEL_INF);

#define TRACKPAD_NODE DT_NODELABEL(trackpad)
#define IQS5XX_ADDR DT_REG_ADDR(TRACKPAD_NODE)

static const struct device *const bus = DEVICE_DT_GET(DT_BUS(TRACKPAD_NODE));
static const struct gpio_dt_spec rdy = GPIO_DT_SPEC_GET(TRACKPAD_NODE, rdy_gpios);

static void probe_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(probe_work, probe_work_handler);

static void probe_work_handler(struct k_work *work) {
    if (!device_is_ready(bus)) {
        LOG_ERR("I2C bus %s not ready", bus->name);
        goto reschedule;
    }

    LOG_INF("Scanning %s, RDY pin = %d", bus->name, gpio_pin_get_raw(rdy.port, rdy.pin));

    int found = 0;
    for (uint16_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t byte;
        if (i2c_read(bus, &byte, 1, addr) == 0) {
            LOG_INF("  ACK at 0x%02x", addr);
            found++;
        }
    }
    LOG_INF("Scan done, %d device(s) found", found);

    /* Product number, project number, major/minor version: 0x0000..0x0005 */
    uint8_t reg[2] = {0x00, 0x00};
    uint8_t id[6];
    int ret = i2c_write_read(bus, IQS5XX_ADDR, reg, sizeof(reg), id, sizeof(id));
    if (ret == 0) {
        LOG_INF("IQS5xx @0x%02x: product %u, project %u, version %u.%u", IQS5XX_ADDR,
                (id[0] << 8) | id[1], (id[2] << 8) | id[3], id[4], id[5]);
    } else {
        LOG_ERR("IQS5xx @0x%02x ID read failed: %d", IQS5XX_ADDR, ret);
    }

    /* Close the communication window the reads opened. */
    uint8_t end_comm[3] = {0xEE, 0xEE, 0x00};
    i2c_write(bus, end_comm, sizeof(end_comm), IQS5XX_ADDR);

reschedule:
    k_work_reschedule(&probe_work, K_SECONDS(10));
}

static int thumble_i2c_probe_init(void) {
    k_work_reschedule(&probe_work, K_SECONDS(3));
    return 0;
}

SYS_INIT(thumble_i2c_probe_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
