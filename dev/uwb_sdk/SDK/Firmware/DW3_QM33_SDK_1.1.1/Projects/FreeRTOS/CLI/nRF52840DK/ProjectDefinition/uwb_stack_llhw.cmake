# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

# LLHW drv definitions
target_compile_definitions(
  ProjectDefinition
  PUBLIC CONFIG_UWB_SPI_INSTANCE=3
         CONFIG_UWB_SPI_FRAME_SIZE=8
         CONFIG_SPI_UWB_IRQ_PRIORITY=3
         CONFIG_SPI_UWB_SCK_GPIO_PORT=1
         CONFIG_SPI_UWB_SCK_GPIO_PIN=15
         CONFIG_SPI_UWB_MOSI_GPIO_PORT=1
         CONFIG_SPI_UWB_MOSI_GPIO_PIN=13
         CONFIG_SPI_UWB_MISO_GPIO_PORT=1
         CONFIG_SPI_UWB_MISO_GPIO_PIN=14
         CONFIG_SPI_UWB_CS_GPIO_PORT=1
         CONFIG_SPI_UWB_CS_GPIO_PIN=12
         CONFIG_DWT_WU_GPIO_PORT=1
         CONFIG_DWT_WU_GPIO_PIN=11
         CONFIG_DWT_IRQ_GPIO_PORT=1
         CONFIG_DWT_IRQ_GPIO_PIN=10
         CONFIG_DWT_RSTN_GPIO_PORT=1
         CONFIG_DWT_RSTN_GPIO_PIN=8
)

# Nordic SDK defintions SPI
target_compile_definitions(
  ProjectDefinition
  PUBLIC NRFX_SPIM_ENABLED=1
         NRFX_SPIM0_ENABLED=1
         NRFX_SPIM1_ENABLED=0
         NRFX_SPIM2_ENABLED=1
         NRFX_SPIM3_ENABLED=1
         NRFX_SPIM_MISO_PULL_CFG=1
         NRFX_SPI_ENABLED=0
         SPI_ENABLED=1
         SPI0_ENABLED=1
         SPI1_ENABLED=0
         SPI2_ENABLED=1
         SPI3_ENABLED=1
         SPI0_USE_EASY_DMA=1
         SPI1_USE_EASY_DMA=1
         SPI2_USE_EASY_DMA=1
         SPI3_USE_EASY_DMA=1
         SPI0_INSTANCE_INDEX=0
         SPI1_INSTANCE_INDEX=0
         SPI2_INSTANCE_INDEX=2
         SPI3_INSTANCE_INDEX=3
         NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY=7
         NRFX_SPIM_MISO_PULL_CFG=1
         NRFX_SPIM_EXTENDED_ENABLED=0
)

# GPIO
target_compile_definitions(
  ProjectDefinition PUBLIC GPIOTE_ENABLED=1 NRFX_GPIOTE_ENABLED=0 NRFX_GPIOTE_CONFIG_IRQ_PRIORITY=6
                           GPIOTE_CONFIG_NUM_OF_LOW_POWER_EVENTS=1
)

# CLOCK
target_compile_definitions(
  ProjectDefinition
  PUBLIC NRFX_CLOCK_ENABLED=1
         NRF_CLOCK_ENABLED=1
         NRFX_CLOCK_CONFIG_LF_SRC=1
         CLOCK_CONFIG_LF_SRC=1
         NRFX_CLOCK_CONFIG_IRQ_PRIORITY=6
         CLOCK_CONFIG_IRQ_PRIORITY=6
         CLOCK_CONFIG_LF_CAL_ENABLED=0
)

# ARM SYSTICK
target_compile_definitions(ProjectDefinition PUBLIC NRFX_SYSTICK_ENABLED=1)

# RTC
target_compile_definitions(
  ProjectDefinition
  PUBLIC RTC_ENABLED=1
         NRFX_RTC_ENABLED=1
         RTC_DEFAULT_CONFIG_FREQUENCY=32768
         NRFX_RTC_DEFAULT_CONFIG_FREQUENCY=32768
         RTC_DEFAULT_CONFIG_RELIABLE=0
         NRFX_RTC_DEFAULT_CONFIG_RELIABLE=0
         RTC_DEFAULT_CONFIG_IRQ_PRIORITY=7
         NRFX_RTC_DEFAULT_CONFIG_IRQ_PRIORITY=7
         NRF_MAXIMUM_LATENCY_US=2000
)

# DTU Base timer - RTC
target_compile_definitions(ProjectDefinition PUBLIC RTC0_ENABLED=1 NRFX_RTC0_ENABLED=1)

# UWB stack Idle - RTC
target_compile_definitions(
  ProjectDefinition PUBLIC RTC2_ENABLED=1 NRFX_RTC2_ENABLED=1
) # RTC1 is used by FreeRTOS

# LOG timestamp Timer
if(CONFIG_LOG)
  target_compile_definitions(ProjectDefinition PUBLIC TIMER4_ENABLED=1 NRFX_TIMER4_ENABLED=1)
else()
  target_compile_definitions(
    ProjectDefinition PUBLIC NRFX_TIMER_ENABLED=0 TIMER4_ENABLED=0 NRFX_TIMER4_ENABLED=0
  )
endif()

# Watchdog
target_compile_definitions(
  ProjectDefinition
  PUBLIC WDT_ENABLED=1
         WDT_CONFIG_BEHAVIOUR=1
         NRFX_WDT_CONFIG_BEHAVIOUR=1
         WDT_CONFIG_RELOAD_VALUE=2000
         NRFX_WDT_CONFIG_NO_IRQ=0
         WDT_CONFIG_IRQ_PRIORITY=7
         NRFX_WDT_CONFIG_IRQ_PRIORITY=7
)

# NMV Flash Controller
target_compile_definitions(ProjectDefinition PUBLIC NRFX_NVMC_ENABLED=1)

# USB Controller
target_compile_definitions(
  ProjectDefinition
  PUBLIC NRFX_USBD_ENABLED=1
         USBD_ENABLED=0
         NRFX_USBD_CONFIG_IRQ_PRIORITY=5
         USBD_CONFIG_IRQ_PRIORITY=5
         NRFX_USBD_CONFIG_DMASCHEDULER_MODE=0
         USBD_CONFIG_DMASCHEDULER_MODE=0
         NRFX_USBD_CONFIG_DMASCHEDULER_ISO_BOOST=1
         USBD_CONFIG_DMASCHEDULER_ISO_BOOST=1
         NRFX_USBD_CONFIG_ISO_IN_ZLP=0
         USBD_CONFIG_ISO_IN_ZLP=0
)

target_compile_definitions(
  ProjectDefinition
  PUBLIC APP_USBD_ENABLED=1
         APP_USBD_VID=0x1915
         APP_USBD_PID=0x520F
         APP_USBD_DEVICE_VER_MAJOR=1
         APP_USBD_DEVICE_VER_MINOR=0
         APP_USBD_DEVICE_VER_SUB=0
         APP_USBD_CONFIG_SELF_POWERED=1
         APP_USBD_CONFIG_MAX_POWER=500
         APP_USBD_CONFIG_POWER_EVENTS_PROCESS=1
         APP_USBD_CONFIG_EVENT_QUEUE_ENABLE=0
         APP_USBD_CONFIG_EVENT_QUEUE_SIZE=32
         APP_USBD_CONFIG_SOF_HANDLING_MODE=1
         APP_USBD_CONFIG_SOF_TIMESTAMP_PROVIDE=0
         APP_USBD_CONFIG_DESC_STRING_SIZE=31
         APP_USBD_CONFIG_DESC_STRING_UTF_ENABLED=0
         APP_USBD_STRING_ID_MANUFACTURER=1
         APP_USBD_STRINGS_MANUFACTURER_EXTERN=0
         APP_USBD_STRINGS_MANUFACTURER=APP_USBD_STRING_DESC\(\"Nordic\ Semiconductor\"\)
         APP_USBD_STRING_ID_PRODUCT=2
         APP_USBD_STRINGS_PRODUCT_EXTERN=0
         APP_USBD_STRINGS_PRODUCT=APP_USBD_STRING_DESC\(\"nRF52\ USB\ Product\"\)
         APP_USBD_STRING_ID_SERIAL=3
         APP_USBD_STRING_SERIAL_EXTERN=1
         APP_USBD_STRING_SERIAL=g_extern_serial_number
         APP_USBD_CDC_ACM_ENABLED=1
         APP_USBD_CDC_ACM_ZLP_ON_EPSIZE_WRITE=1
         NRF_CLI_ENABLED=0
)

if(CONFIG_LOG)
  # define DEBUG level filtering to minimum, will be filtered by QOSAL in the end.
  target_compile_definitions(
    ProjectDefinition
    PUBLIC CONFIG_LOG=1
           NRF_LOG_ENABLED=1
           NRF_LOG_USES_COLORS=1
           NRF_LOG_STR_FORMATTER_TIMESTAMP_FORMAT_ENABLED=1
           NRF_LOG_USES_TIMESTAMP=1
           NRF_LOG_FILTERS_ENABLED=0
           NRF_LOG_BACKEND_RTT_ENABLED=1
           # use SysTick from FreeRTOS as log timestamp.
           NRF_LOG_TIMESTAMP_DEFAULT_FREQUENCY=1000000
           # define DEBUG level filtering to minimum, will be filtered by QOSAL in the end.
           NRF_LOG_DEFAULT_LEVEL=4
           NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE=1024
           NRF_LOG_STR_PUSH_BUFFER_SIZE=1024
           NRF_LOG_BACKEND_RTT_TX_RETRY_DELAY_MS=1
           NRF_LOG_BACKEND_RTT_TX_RETRY_CNT=3
           SEGGER_RTT_CONFIG_BUFFER_SIZE_UP=2048
           SEGGER_RTT_CONFIG_MAX_NUM_UP_BUFFERS=5
           SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN=2048
           SEGGER_RTT_CONFIG_MAX_NUM_DOWN_BUFFERS=5
           SEGGER_RTT_CONFIG_DEFAULT_MODE=0
           NRF_LOG_MSGPOOL_ELEMENT_SIZE=50
           NRF_LOG_MSGPOOL_ELEMENT_COUNT=80
           NRF_LOG_ALLOW_OVERFLOW=0
           NRF_LOG_BUFSIZE=4096
           NRF_LOG_DEFERRED=1
  )

  # <0=> Default <1=> Black <2=> Red <3=> Green <4=> Yellow <5=> Blue <6=> Magenta <7=> Cyan <8=>
  # White
  target_compile_definitions(
    ProjectDefinition PUBLIC NRF_LOG_COLOR_DEFAULT=0 NRF_LOG_INFO_COLOR=3 NRF_LOG_WARNING_COLOR=4
                             NRF_LOG_ERROR_COLOR=2
  )

  if(CONFIG_QLOG_LEVEL)
    target_compile_definitions(ProjectDefinition PUBLIC CONFIG_QLOG_LEVEL=${CONFIG_QLOG_LEVEL})
  else()
    # define QLOG_LEVEL level filtering to INFO by default.
    target_compile_definitions(ProjectDefinition PUBLIC CONFIG_QLOG_LEVEL=2)
  endif()
endif()
