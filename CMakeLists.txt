# SPDX-License-Identifier: MIT

if(CONFIG_THUMBLE_I2C_PROBE)
  zephyr_library()
  zephyr_library_sources(src/i2c_probe.c)
endif()
