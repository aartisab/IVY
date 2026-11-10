/**
 * @file      nRF52840DK.c
 *
 * @brief     Board specific initialization
 *
 * @author    Qorvo Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024-2025 Qorvo US, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "app_error.h"
#include "thisBoard.h"
#include "boards.h"
#include "nrf_drv_clock.h"
#include "HAL_error.h"
#if defined(USB_ENABLE)
#include "HAL_usb.h"
#endif
#include "usb_uart_rx.h"
#include "driver_app_config.h"
#include "HAL_uart.h"
#include "HAL_watchdog.h"
#include "HAL_cpu.h"
#include "controlTask.h"
#include "qerr.h"
#include "llhw.h"
#include "persistent_time.h"
#include "nrfx_nvmc.h"
#include "sdk_config.h"
#include "nrfx_spis.h"
#include "nrf_gpio.h"

#define ESP_SPI_SCK_PIN  NRF_GPIO_PIN_MAP(1, 3)  // D2
#define ESP_SPI_MOSI_PIN NRF_GPIO_PIN_MAP(1, 4)  // D3
#define ESP_SPI_MISO_PIN NRF_GPIO_PIN_MAP(1, 5)  // D4
#define ESP_SPI_CS_PIN   NRF_GPIO_PIN_MAP(1, 6)  // D5

static const nrfx_spis_t esp_spi = NRFX_SPIS_INSTANCE(1);

static uint8_t spi_tx_buf[4] = {0};
static uint8_t spi_rx_buf[4] = {0};

static void esp_spi_event_handler(nrfx_spis_evt_t const *p_event,
                                  void *p_context)
{
    if (p_event->evt_type == NRFX_SPIS_XFER_DONE)
    {
        ret_code_t err = nrfx_spis_buffers_set(
            &esp_spi,
            spi_tx_buf,
            sizeof(spi_tx_buf),
            spi_rx_buf,
            sizeof(spi_rx_buf)
        );

        APP_ERROR_CHECK(err);
    }
}

/** @brief Initialize peripherals. */
void peripherals_init(void)
{
    nrfx_spis_config_t spi_config = NRFX_SPIS_DEFAULT_CONFIG;

    spi_config.sck_pin  = ESP_SPI_SCK_PIN;
    spi_config.mosi_pin = ESP_SPI_MOSI_PIN;
    spi_config.miso_pin = ESP_SPI_MISO_PIN;
    spi_config.csn_pin  = ESP_SPI_CS_PIN;
    spi_config.mode     = NRF_SPIS_MODE_0;

    ret_code_t spi_err = nrfx_spis_init(
        &esp_spi,
        &spi_config,
        esp_spi_event_handler,
        NULL
    );

    APP_ERROR_CHECK(spi_err);

    spi_err = nrfx_spis_buffers_set(
        &esp_spi,
        spi_tx_buf,
        sizeof(spi_tx_buf),
        spi_rx_buf,
        sizeof(spi_rx_buf)
    );

    APP_ERROR_CHECK(spi_err);
    
    /* With this change, Reset After Power Cycle is not required. */
    nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);

    nrfx_nvmc_icache_enable();

    ret_code_t ret = nrf_drv_clock_init();
    if ((ret != NRF_SUCCESS) && (ret != NRF_ERROR_MODULE_ALREADY_INITIALIZED))
    {
        error_handler(1, _ERR);
    }

    /* Start LFCLK for proper operation of the RTC. */
    nrfx_clock_lfclk_start();
    while (!nrfx_clock_lfclk_is_running())
        ;

#if !defined(USE_USB_ENUM_WORKAROUND)
    /* Enable USE_USB_ENUM_WORKAROUND to fix long USB enumeration.
     * If HDK is battery supplied, the flag USE_USB_ENUM_WORKAROUND should be turned off.

     * Reasoning: if we turn on HFCLK before USB is enumerated, it will hang for up to 14 seconds.
     * When USE_USB_ENUM_WORKAROUND flag is on, then HFCLK request is being handled in the HAL_usb.c.
     * When USB peripheral is not used but HDK us supplied by a USB power bank, then HFCLK may not be
     * turned on properly, in such case USE_USB_ENUM_WORKAROUND should be turned off. */
    nrf_drv_clock_hfclk_request(NULL);
    while (!nrf_drv_clock_hfclk_is_running())
        ;
#endif

    /* Watchdog 60sec */
    Watchdog.init(BOARD_WD_INIT_TIME);
}

void BoardInit(void)
{
    handle_cpu_protect();
    bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);
    peripherals_init();
}

void board_interface_init(void)
{
#if defined(USB_ENABLE)
    Usb.init(&NotifyControlTaskData);
#endif
    if (is_uart_allowed())
    {
        deca_uart_init(&NotifyControlTaskData);
    }
    persistent_time_init(0);
}

void uwb_init(void)
{
    persistent_time_init(0);
}
