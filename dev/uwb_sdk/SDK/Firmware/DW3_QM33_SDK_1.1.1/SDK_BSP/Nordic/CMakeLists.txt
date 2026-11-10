# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

set(SDK_VERSION SDK_17_1_0)
set(SDK_SRC
    ${SDK_VERSION}/components/boards/boards.c
    ${SDK_VERSION}/components/libraries/fifo/app_fifo.c
    ${SDK_VERSION}/components/libraries/uart/app_uart_fifo.c
    ${SDK_VERSION}/components/libraries/util/app_util_platform.c
    ${SDK_VERSION}/components/libraries/util/app_error.c
    ${SDK_VERSION}/components/libraries/util/app_error_weak.c
    ${SDK_VERSION}/components/libraries/atomic/nrf_atomic.c
    ${SDK_VERSION}/integration/nrfx/legacy/nrf_drv_spi.c
    ${SDK_VERSION}/integration/nrfx/legacy/nrf_drv_clock.c
    ${SDK_VERSION}/integration/nrfx/legacy/nrf_drv_power.c
    ${SDK_VERSION}/integration/nrfx/legacy/nrf_drv_uart.c
    ${SDK_VERSION}/integration/nrfx/legacy/nrf_drv_rng.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_rng.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_gpiote.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_rtc.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_spi.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_spim.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_uart.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_uarte.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_power.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_wdt.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_clock.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_nvmc.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/prs/nrfx_prs.c
    ${SDK_VERSION}/modules/nrfx/hal/nrf_nvmc.c
    ${SDK_VERSION}/modules/nrfx/soc/nrfx_atomic.c
    ${SDK_VERSION}/components/libraries/queue/nrf_queue.c
    # Only for debugging
    ${SDK_VERSION}/components/libraries/strerror/nrf_strerror.c
)

if(NRFX_TIMER_ENABLED EQUAL 1)
  list(APPEND SDK_SRC ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_timer.c)
endif()

set(SDK_RTT_SRC
    ${SDK_VERSION}/components/libraries/log/src/nrf_log_backend_rtt.c
    ${SDK_VERSION}/components/libraries/log/src/nrf_log_default_backends.c
    ${SDK_VERSION}/components/libraries/log/src/nrf_log_backend_serial.c
    ${SDK_VERSION}/components/libraries/log/src/nrf_log_frontend.c
    ${SDK_VERSION}/components/libraries/log/src/nrf_log_str_formatter.c
    ${SDK_VERSION}/external/segger_rtt/SEGGER_RTT.c
    ${SDK_VERSION}/external/segger_rtt/SEGGER_RTT_Syscalls_SES.c
    ${SDK_VERSION}/external/segger_rtt/SEGGER_RTT_printf.c
    ${SDK_VERSION}/components/libraries/memobj/nrf_memobj.c
    ${SDK_VERSION}/components/libraries/balloc/nrf_balloc.c
    ${SDK_VERSION}/components/libraries/ringbuf/nrf_ringbuf.c
    ${SDK_VERSION}/components/libraries/atomic/nrf_atomic.c
    ${SDK_VERSION}/external/fprintf/nrf_fprintf.c
    ${SDK_VERSION}/external/fprintf/nrf_fprintf_format.c
)

set(SDK_INC
    ${SDK_VERSION}/integration/nrfx/legacy
    ${SDK_VERSION}/modules/nrfx
    ${SDK_VERSION}/modules/nrfx/hal
    ${SDK_VERSION}/modules/nrfx/drivers/include
    ${SDK_VERSION}/components/boards
    ${SDK_VERSION}/components/libraries/atomic
    ${SDK_VERSION}/components/libraries/delay
    ${SDK_VERSION}/components/libraries/fifo
    ${SDK_VERSION}/components/libraries/log
    ${SDK_VERSION}/components/libraries/log/src
    ${SDK_VERSION}/components/libraries/uart
    ${SDK_VERSION}/components/libraries/usbd
    ${SDK_VERSION}/components/libraries/timer
    ${SDK_VERSION}/components/libraries/usbd/class/cdc
    ${SDK_VERSION}/components/libraries/usbd/class/cdc/acm
    ${SDK_VERSION}/components/libraries/experimental_section_vars
    ${SDK_VERSION}/components/libraries/strerror
    ${SDK_VERSION}/modules/nrfx/mdk
    ${SDK_VERSION}/integration/nrfx
    ${SDK_VERSION}/components/toolchain/cmsis/include
    ${SDK_VERSION}/components/libraries/util
    ${SDK_VERSION}/components/drivers_nrf/nrf_soc_nosd
    ${SDK_VERSION}/external/utf_converter
    ${SDK_VERSION}/external/mbedtls/include
    ${SDK_VERSION}/components/libraries/fds
)

set(SDK_RTT_INC
    ${SDK_VERSION}/components/libraries/log/
    ${SDK_VERSION}/external/segger_rtt
    ${SDK_VERSION}/components/libraries/memobj
    ${SDK_VERSION}/components/libraries/balloc
    ${SDK_VERSION}/external/fprintf
    ${SDK_VERSION}/components/libraries/ringbuf
    ${SDK_VERSION}/components/libraries/memobj
)

set(SYSTICK_SRC ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_systick.c)
set(SYSTICK_INC ${SDK_VERSION}/modules/nrfx/drivers/include ${SDK_VERSION}/modules/nrfx)

set(SDK_USB_SRC
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_usbd.c
    ${SDK_VERSION}/modules/nrfx/drivers/src/nrfx_systick.c
    ${SDK_VERSION}/components/libraries/usbd/app_usbd.c
    ${SDK_VERSION}/components/libraries/usbd/app_usbd_core.c
    ${SDK_VERSION}/components/libraries/usbd/app_usbd_serial_num.c
    ${SDK_VERSION}/components/libraries/usbd/app_usbd_string_desc.c
    ${SDK_VERSION}/components/libraries/usbd/class/cdc/acm/app_usbd_cdc_acm.c
    ${SDK_VERSION}/components/libraries/uart/app_uart_fifo.c
)

if(USE_SOFTDEVICE EQUAL 1)
  set(SDK_SRC
      ${SDK_SRC}
      ${SDK_VERSION}/components/libraries/button/app_button.c
      ${SDK_VERSION}/components/libraries/util/app_error_handler_gcc.c
      ${SDK_VERSION}/components/libraries/timer/app_timer_freertos.c
      ${SDK_VERSION}/components/libraries/fds/fds.c
      ${SDK_VERSION}/components/libraries/hardfault/nrf52/handler/hardfault_handler_gcc.c
      ${SDK_VERSION}/components/libraries/hardfault/hardfault_implementation.c
      ${SDK_VERSION}/components/libraries/util/nrf_assert.c
      ${SDK_VERSION}/components/libraries/atomic_fifo/nrf_atfifo.c
      ${SDK_VERSION}/components/libraries/atomic_flags/nrf_atflags.c
      ${SDK_VERSION}/components/libraries/fstorage/nrf_fstorage.c
      ${SDK_VERSION}/components/libraries/fstorage/nrf_fstorage_sd.c
      ${SDK_VERSION}/components/libraries/experimental_section_vars/nrf_section_iter.c
      ${SDK_VERSION}/components/libraries/strerror/nrf_strerror.c
      ${SDK_VERSION}/components/libraries/sensorsim/sensorsim.c
      ${SDK_VERSION}/components/libraries/fifo/app_fifo.c
  )

  set(SDK_LIB_INC
      ${SDK_VERSION}/components/libraries/atomic
      ${SDK_VERSION}/components/libraries/atomic_fifo
      ${SDK_VERSION}/components/libraries/atomic_flags
      ${SDK_VERSION}/components/libraries/balloc
      ${SDK_VERSION}/components/libraries/bootloader/ble_dfu
      ${SDK_VERSION}/components/libraries/bsp
      ${SDK_VERSION}/components/libraries/button
      ${SDK_VERSION}/components/libraries/cli
      ${SDK_VERSION}/components/libraries/crc32
      ${SDK_VERSION}/components/libraries/crypto
      ${SDK_VERSION}/components/libraries/csense
      ${SDK_VERSION}/components/libraries/csense_drv
      ${SDK_VERSION}/components/libraries/delay
      ${SDK_VERSION}/components/libraries/ecc
      ${SDK_VERSION}/components/libraries/experimental_section_vars
      ${SDK_VERSION}/components/libraries/experimental_task_manager
      ${SDK_VERSION}/components/libraries/fds
      ${SDK_VERSION}/components/libraries/fifo
      ${SDK_VERSION}/components/libraries/fstorage
      ${SDK_VERSION}/components/libraries/gfx
      ${SDK_VERSION}/components/libraries/gpiote
      ${SDK_VERSION}/components/libraries/hardfault
      ${SDK_VERSION}/components/libraries/hardfault/nrf52
      ${SDK_VERSION}/components/libraries/hci
      ${SDK_VERSION}/components/libraries/led_softblink
      ${SDK_VERSION}/components/libraries/log
      ${SDK_VERSION}/components/libraries/log/src
      ${SDK_VERSION}/components/libraries/low_power_pwm
      ${SDK_VERSION}/components/libraries/mem_manager
      ${SDK_VERSION}/components/libraries/memobj
      ${SDK_VERSION}/components/libraries/mpu
      ${SDK_VERSION}/components/libraries/mutex
      ${SDK_VERSION}/components/libraries/pwm
      ${SDK_VERSION}/components/libraries/pwr_mgmt
      ${SDK_VERSION}/components/libraries/queue
      ${SDK_VERSION}/components/libraries/ringbuf
      ${SDK_VERSION}/components/libraries/scheduler
      ${SDK_VERSION}/components/libraries/sdcard
      ${SDK_VERSION}/components/libraries/sensorsim
      ${SDK_VERSION}/components/libraries/stack_info
      ${SDK_VERSION}/components/libraries/slip
      ${SDK_VERSION}/components/libraries/sortlist
      ${SDK_VERSION}/components/libraries/spi_mngr
      ${SDK_VERSION}/components/libraries/stack_guard
      ${SDK_VERSION}/components/libraries/strerror
      ${SDK_VERSION}/components/libraries/svc
      ${SDK_VERSION}/components/libraries/timer
      ${SDK_VERSION}/components/libraries/twi_mngr
      ${SDK_VERSION}/components/libraries/twi_sensor
      ${SDK_VERSION}/components/libraries/uart
      ${SDK_VERSION}/components/libraries/stack_info
  )
endif()

# Soft device is here for now, but this is a problem. it is especially mentionning free RTOS, etc...
if(USE_SOFTDEVICE EQUAL 1)
  set(SDK_SRC
      ${SDK_SRC}
      ${SDK_VERSION}/components/softdevice/common/nrf_sdh.c
      ${SDK_VERSION}/components/softdevice/common/nrf_sdh_ble.c
      ${SDK_VERSION}/components/softdevice/common/nrf_sdh_freertos.c
      # To overcome SDK link issue, we need to include this out of nRF SDK.
      custom/nrf_sdh_soc.c
      ${SDK_VERSION}/components/libraries/log/src/nrf_log_frontend.c
      ${SDK_VERSION}/components/libraries/log/src/nrf_log_default_backends.c
      ${SDK_VERSION}/components/libraries/log/src/nrf_log_backend_rtt.c
      ${SDK_VERSION}/components/libraries/log/src/nrf_log_backend_serial.c
      ${SDK_VERSION}/components/libraries/log/src/nrf_log_str_formatter.c
      ${SDK_VERSION}/components/libraries/ringbuf/nrf_ringbuf.c
      ${SDK_VERSION}/components/libraries/memobj/nrf_memobj.c
      ${SDK_VERSION}/components/libraries/balloc/nrf_balloc.c
      ${SDK_VERSION}/external/fprintf/nrf_fprintf.c
      ${SDK_VERSION}/external/fprintf/nrf_fprintf_format.c
      ${SDK_VERSION}/external/segger_rtt/SEGGER_RTT.c
      ${SDK_VERSION}/external/segger_rtt/SEGGER_RTT_printf.c
      ${SDK_VERSION}/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c
  )
  set(SDK_INC
      # we must have soft device includes before SDK incluse, for nrf_error.h in
      # particular....bad...
      ${SDK_VERSION}/components/softdevice/common
      ${SDK_VERSION}/components/softdevice/s113/headers
      ${SDK_VERSION}/components/softdevice/s113/headers/nrf52
      ${SDK_VERSION}/components/softdevice/mbr/headers
      ${SDK_VERSION}/components/libraries/log
      ${SDK_VERSION}/components/libraries/log/src
      ${SDK_VERSION}/components/libraries/util
      ${SDK_VERSION}/components/libraries/strerror
      ${SDK_VERSION}/components/libraries/experimental_section_vars
      ${SDK_VERSION}/components/libraries/memobj
      ${SDK_VERSION}/components/libraries/balloc
      ${SDK_VERSION}/components/libraries/ringbuf
      ${SDK_VERSION}/components/toolchain/cmsis/include
      ${SDK_VERSION}/external/freertos/source/include
      ${SDK_VERSION}/external/freertos/portable/ARM/nrf52
      ${SDK_VERSION}/external/freertos/portable/CMSIS/nrf52
      ${SDK_VERSION}/external/fprintf/
      ${SDK_VERSION}/external/segger_rtt/
      ${SDK_VERSION}/modules/nrfx/mdk
      ${SDK_INC}
  )
endif()

if(${MY_CPU} STREQUAL NRF52833_XXAA)
  set(SDK_SRC
      ${SDK_SRC} ${SDK_USB_SRC} ${SDK_LIBS_SRC}
      ${SDK_VERSION}/modules/nrfx/mdk/gcc_startup_nrf52833.S
      ${SDK_VERSION}/modules/nrfx/mdk/system_nrf52833.c
  )
endif()

if(${MY_CPU} STREQUAL NRF52840_XXAA)
  set(SDK_SRC ${SDK_SRC} ${SDK_USB_SRC} ${SDK_VERSION}/modules/nrfx/mdk/gcc_startup_nrf52840.S
              ${SDK_VERSION}/modules/nrfx/mdk/system_nrf52840.c
  )
endif()

add_library(sdk_systick ${SYSTICK_SRC})
target_include_directories(
  sdk_systick
  PUBLIC ${SYSTICK_INC}
  PRIVATE ${SDK_INC} ../../Src/Boards/Src/${MY_BOARD}/Common
)
target_link_libraries(sdk_systick PUBLIC ProjectDefinition)

if(CONFIG_LOG)
  add_library(log_rtt ${SDK_RTT_SRC})

  target_include_directories(
    log_rtt
    PUBLIC ${SDK_RTT_INC}
    PRIVATE ${SDK_INC}
  )

  target_link_libraries(log_rtt PUBLIC ProjectDefinition)

  install(TARGETS log_rtt EXPORT log_rtt-config)
  install(EXPORT log_rtt-config DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE}/log_rtt/cmake)
endif(CONFIG_LOG)

set(SDK_INC ${SDK_INC} ${SDK_LIB_INC})

add_library(SDK ${SDK_SRC})

target_include_directories(
  SDK
  PUBLIC ${SDK_INC}
  PRIVATE ./${SDK_VERSION} ../../Src/Boards/Src/${MY_BOARD}/Common
)

if(USE_BOARD_LIB)
  target_link_libraries(SDK ProjectDefinition Board)
else()
  target_link_libraries(SDK ProjectDefinition)
endif()

if(CONFIG_LOG)
  target_link_libraries(SDK log_rtt)
endif()

target_compile_definitions(
  SDK PUBLIC MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/custom/config.h"
)

# Crypto sources and config
set(SDK_CRYPTO_INC
    ${SDK_VERSION}/components/libraries/crypto
    ${SDK_VERSION}/components/libraries/crypto/backend/cc310
    ${SDK_VERSION}/components/libraries/crypto/backend/mbedtls
    ${SDK_VERSION}/components/libraries/crypto/backend/cifra
    ${SDK_VERSION}/components/libraries/crypto/backend/oberon
    ${SDK_VERSION}/components/libraries/crypto/backend/optiga
    ${SDK_VERSION}/components/libraries/crypto/backend/nrf_hw
    ${SDK_VERSION}/external/nrf_cc310_bl/include
    ${SDK_VERSION}/external/nrf_cc310/include
    ${SDK_VERSION}/external/mbedtls/include/mbedtls
    ${SDK_VERSION}/components/libraries/mutex
    ${SDK_VERSION}/components/libraries/queue
)

set(SDK_CRYPTO_SRC
    ${SDK_VERSION}/components/libraries/crypto/nrf_crypto_aes.c
    ${SDK_VERSION}/components/libraries/crypto/nrf_crypto_init.c
    ${SDK_VERSION}/components/libraries/crypto/nrf_crypto_aead.c
    ${SDK_VERSION}/components/libraries/crypto/nrf_crypto_shared.c
    ${SDK_VERSION}/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_init.c
    ${SDK_VERSION}/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng.c
    # To workaround SDK bug, we need to include this out of nRF SDK.
    custom/nrf_crypto_rng.c
)

if(USE_CRYPTO_BACKEND_CC310)
  set(SDK_CRYPTO_BACKEND_SRC
      ${SDK_VERSION}/components/libraries/crypto/backend/cc310/cc310_backend_init.c
      ${SDK_VERSION}/components/libraries/crypto/backend/cc310/cc310_backend_aes.c
      ${SDK_VERSION}/components/libraries/crypto/backend/cc310/cc310_backend_aes_aead.c
      ${SDK_VERSION}/components/libraries/crypto/backend/cc310/cc310_backend_mutex.c
  )
  target_link_libraries(
    SDK
    ${CMAKE_CURRENT_SOURCE_DIR}/${SDK_VERSION}/external/nrf_cc310/lib/cortex-m4/hard-float/libnrf_cc310_0.9.13.a
  )
  target_compile_definitions(SDK PUBLIC NRF_CRYPTO_BACKEND_CC310_ENABLED=1)
elseif(USE_CRYPTO_BACKEND_MBEDTLS)
  set(SDK_CRYPTO_BACKEND_SRC
      ${SDK_VERSION}/external/mbedtls/library/platform.c
      ${SDK_VERSION}/components/libraries/crypto/backend/mbedtls/mbedtls_backend_init.c
      ${SDK_VERSION}/components/libraries/crypto/backend/mbedtls/mbedtls_backend_aes.c
      ${SDK_VERSION}/components/libraries/crypto/backend/mbedtls/mbedtls_backend_aes_aead.c
      ${SDK_VERSION}/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_init.c
      ${SDK_VERSION}/external/mbedtls/library/aes.c
      ${SDK_VERSION}/external/mbedtls/library/ccm.c
      ${SDK_VERSION}/external/mbedtls/library/gcm.c
      ${SDK_VERSION}/external/mbedtls/library/cipher.c
      ${SDK_VERSION}/external/mbedtls/library/cipher_wrap.c
      ${SDK_VERSION}/external/mbedtls/library/cmac.c
      ${SDK_VERSION}/external/mbedtls/library/platform_util.c
      ${SDK_VERSION}/external/mbedtls/library/ctr_drbg.c
  )
  target_compile_definitions(SDK PUBLIC NRF_CRYPTO_BACKEND_MBEDTLS_ENABLED=1)
else()
  message(WARNING "No crypto backend chosen!")
endif()

if(USE_CRYPTO_SPEED_OPTIMIZATION)
  foreach(source_file ${SDK_CRYPTO_BACKEND_SRC})
    set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS "-O3")
  endforeach()
endif()

target_sources(SDK PRIVATE ${SDK_CRYPTO_SRC} ${SDK_CRYPTO_BACKEND_SRC})
target_include_directories(SDK PUBLIC ${SDK_CRYPTO_INC})

# Prevent crypto backends from being optimized out.
target_link_options(
  SDK PUBLIC -Wl,--undefined=mbedtls_backend -Wl,--undefined=cc310_backend
  -Wl,--undefined=nrf_hw_backend -Wl,--undefined=nrf_sdh_soc_evts_poll
)

install(TARGETS SDK EXPORT SDK-config)
install(EXPORT SDK-config DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE}/SDK/cmake)
