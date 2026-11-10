# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

set(MY_BOARD nRF52840DK)
set(MY_OS FreeRTOS)
set(MY_SAMPLE "CLI") # Sample name consitent with the sample directory name
set(MY_HAL nrfx)
set(MY_BSP Nordic)
set(MY_CPU NRF52840_XXAA)
set(MY_LD_FILE nRF52840.ld)

set(CMAKE_CUSTOM_C_FLAGS
    "-Werror \
    -DBOARD_CUSTOM \
    -DUSB_ENABLE \
    -DCONFIG_GPIO_AS_PINRESET"
)

set(MY_TARGET ${MY_BOARD}-${MY_SAMPLE}-${MY_OS})

add_definitions(-Os)

# Enable USE_USB_ENUM_WORKAROUND to workaround long USB enumeration. If HDK is battery supplied, the
# flag USB_ENUM_WORKAROUND should be removed.
add_definitions(-DUSE_USB_ENUM_WORKAROUND)

# Processor related defines
set(PROJECT_ARCH "m4")
set(PROJECT_FP "hard")
set(PROJECT_FPU "fpv4-sp-d16")

set(USE_DRV_DW3000 0)
set(USE_DRV_DW3720 1)

if(USE_DRV_DW3000 AND USE_DRV_DW3720)
  message(
    WARNING
      "Enabling DW3000 and DW3720 drivers at the same time is allowed for nRF52840, as the proper \
                     driver will be selected at the runtime. However this will increase the binary size."
  )
endif()

set(USE_CRYPTO_BACKEND_MBEDTLS 1)
set(USE_CRYPTO_SPEED_OPTIMIZATION 0)
