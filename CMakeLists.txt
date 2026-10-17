# SPDX-License-Identifier: MIT

if(CONFIG_THUMBLE_I2C_PROBE OR CONFIG_THUMBLE_IQS5XX_RESOLUTION)
  zephyr_library()
  zephyr_library_sources_ifdef(CONFIG_THUMBLE_I2C_PROBE src/i2c_probe.c)
  zephyr_library_sources_ifdef(CONFIG_THUMBLE_IQS5XX_RESOLUTION src/iqs5xx_resolution.c)
endif()
