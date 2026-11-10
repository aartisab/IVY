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

/** @brief Initialize peripherals. */
void peripherals_init(void)
{
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
