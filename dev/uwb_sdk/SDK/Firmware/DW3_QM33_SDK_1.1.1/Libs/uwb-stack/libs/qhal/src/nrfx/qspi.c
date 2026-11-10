/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo US, Inc.
 *
 * SPDX-License-Identifier: LicenseRef-Qorvo-2
 */

#ifdef __ZEPHYR__
#include <hal/nrf_gpio.h>
#else
#include "nrf_gpio.h"
#endif
#include <qgpio.h>
#include <qlog.h>
#include <qspi.h>
#include <stdbool.h>
#include <string.h>

#if NRFX_SPI_ENABLED
#include "nrfx_spi.h"
#elif NRFX_SPIM_ENABLED
#include "nrfx_spim.h"
#endif

#if NRFX_SPIS_ENABLED
#include "nrfx_spim.h"
#endif

#if !(defined(NRFX_SPI_ENABLED) || defined(NRFX_SPIM_ENABLED)) && !defined(NRFX_SPIS_ENABLED)
#error "No SPI implementation selected"
#endif

#if NRFX_SPIM_ENABLED && NRFX_SPI_ENABLED
#error "SPIM and SPI can't be enabled at same time"
#endif

#define QSPI_DEFAULT_ORC 0xFF

#if NRFX_SPIM_ENABLED
#define QSPI_MAX (NRFX_SPIM_ENABLED_COUNT)
#elif NRFX_SPIS_ENABLED
#define QSPI_MAX (NRFX_SPIS_ENABLED_COUNT)
#else
#define QSPI_MAX (NRFX_SPI_ENABLED_COUNT)
#endif

typedef union {
#if NRFX_SPIM_ENABLED
	nrfx_spim_t spim;
#else
	nrfx_spi_t spi;
#endif
#if NRFX_SPIS_ENABLED
	nrfx_spis_t spis;
#endif
} qspi_nrf_spi_t;

typedef union {
#if NRFX_SPIM_ENABLED
	nrfx_spim_config_t master;
#else
	nrfx_spi_config_t master;
#endif
#if NRFX_SPIS_ENABLED
	nrfx_spis_config_t slave;
#endif
} qspi_nrf_spi_config_t;

struct qspi {
	uint8_t instance;
	qspi_nrf_spi_t type;
	qspi_nrf_spi_config_t config;
	struct qgpio *cs_pin;
	bool ss_active_high;
	volatile bool txf_is_finished;
	bool is_configured;
	bool is_used;

	qspi_xfer_cb handler;
	void *arg;

	bool is_master;
};

static struct qspi local_instance_spi[QSPI_MAX] = { 0 };

#if NRFX_SPIM_ENABLED
static void qspi_generic_handler_master(const nrfx_spim_evt_t *p_event, void *p_context)
#else
static void qspi_generic_handler_master(const nrfx_spi_evt_t *p_event, void *p_context)
#endif
{
	struct qspi *spi = (struct qspi *)p_context;

#if NRFX_SPIM_ENABLED
	if ((p_event->type == NRFX_SPIM_EVENT_DONE) && spi)
#else
	if ((p_event->type == NRFX_SPI_EVENT_DONE) && spi)
#endif
	{
		spi->txf_is_finished = true;

		if (spi->handler) {
			spi->handler(spi->arg);
		}

#if NRFX_SPIM_ENABLED
		nrf_spim_disable(spi->type.spim.p_reg);
#else
		if (spi->ss_active_high) {
			qgpio_pin_write(spi->cs_pin, 0);
		} else {
			qgpio_pin_write(spi->cs_pin, 1);
		}

		nrf_spi_disable(spi->type.spi.p_reg);
#endif
	}
}

#if NRFX_SPIS_ENABLED
static void qspi_generic_handler_slave(const nrfx_spis_evt_t *p_event, void *p_context)
{
	struct qspi *spi = (struct qspi *)p_context;

	if ((p_event->evt_type == NRFX_SPIS_XFER_DONE) && spi) {
		spi->txf_is_finished = true;

		if (spi->handler) {
			spi->handler(spi->arg);
		}

		nrf_spis_disable(spi->type.spis.p_reg);
	}
}
#endif

#if NRFX_SPIM_ENABLED
static nrf_spim_frequency_t qspi_freq_to_nrf(uint32_t freq_hz)
{
	if (freq_hz >= 32000000) {
		return NRF_SPIM_FREQ_32M;
	} else if (freq_hz >= 16000000) {
		return NRF_SPIM_FREQ_16M;
	} else if (freq_hz >= 8000000) {
		return NRF_SPIM_FREQ_8M;
	} else if (freq_hz >= 4000000) {
		return NRF_SPIM_FREQ_4M;
	} else if (freq_hz >= 2000000) {
		return NRF_SPIM_FREQ_2M;
	} else if (freq_hz >= 1000000) {
		return NRF_SPIM_FREQ_1M;
	} else if (freq_hz >= 500000) {
		return NRF_SPIM_FREQ_500K;
	} else if (freq_hz >= 250000) {
		return NRF_SPIM_FREQ_250K;
	}

	return NRF_SPIM_FREQ_125K;
}
#else
static nrf_spi_frequency_t qspi_freq_to_nrf(uint32_t freq_hz)
{
	if (freq_hz >= 8000000) {
		return NRF_SPI_FREQ_8M;
	} else if (freq_hz >= 4000000) {
		return NRF_SPI_FREQ_4M;
	} else if (freq_hz >= 2000000) {
		return NRF_SPI_FREQ_2M;
	} else if (freq_hz >= 1000000) {
		return NRF_SPI_FREQ_1M;
	} else if (freq_hz >= 500000) {
		return NRF_SPI_FREQ_500K;
	} else if (freq_hz >= 250000) {
		return NRF_SPI_FREQ_250K;
	}

	return NRF_SPI_FREQ_125K;
}
#endif /* NRFX_SPIM_ENABLED */

#if NRFX_SPIM_ENABLED
static nrf_spim_mode_t qspi_master_mode_to_nrf(uint32_t flags)
{
	switch (flags & (QSPI_CPOL | QSPI_CPHA)) {
	case (QSPI_CPOL | QSPI_CPHA):
		return NRF_SPIM_MODE_1;
	case QSPI_CPOL:
		return NRF_SPIM_MODE_0;
	case QSPI_CPHA:
		return NRF_SPIM_MODE_3;
	default:
		break;
	}

	return NRF_SPIM_MODE_2;
}
#else
static nrf_spi_mode_t qspi_master_mode_to_nrf(uint32_t flags)
{
	switch (flags & (QSPI_CPOL | QSPI_CPHA)) {
	case (QSPI_CPOL | QSPI_CPHA):
		return NRF_SPI_MODE_1;
	case QSPI_CPOL:
		return NRF_SPI_MODE_0;
	case QSPI_CPHA:
		return NRF_SPI_MODE_3;
	default:
		break;
	}

	return NRF_SPI_MODE_2;
}
#endif /* NRFX_SPIM_ENABLED */

#if NRFX_SPIS_ENABLED
static nrf_spis_mode_t qspi_slave_mode_to_nrf(uint32_t flags)
{
	switch (flags & (QSPI_CPOL | QSPI_CPHA)) {
	case (QSPI_CPOL | QSPI_CPHA):
		return NRF_SPIS_MODE_1;
	case QSPI_CPOL:
		return NRF_SPIS_MODE_0;
	case QSPI_CPHA:
		return NRF_SPIS_MODE_3;
	default:
		break;
	}

	return NRF_SPIS_MODE_2;
}
#endif /* NRFX_SPIM_ENABLED */

static struct qspi *find_available_spi(uint8_t spi_instance)
{
	uint8_t idx;

	for (idx = 0; idx < QSPI_MAX; idx++) {
		if ((local_instance_spi[idx].instance == spi_instance) ||
		    !local_instance_spi[idx].is_used) {
			return &local_instance_spi[idx];
		}
	}

	return NULL;
}

static void spi_deconfig(struct qspi *const spi)
{
	spi->is_configured = false;

	/* Uninit spi. */
	if (spi->is_master) {
#if NRFX_SPIM_ENABLED
		nrfx_spim_uninit(&spi->type.spim);
#else
		nrfx_spi_uninit(&spi->type.spi);
#endif
	}
#if NRFX_SPIS_ENABLED
	else {
		nrfx_spis_uninit(&spi->type.spis);
	}
#endif
}

struct qspi *qspi_open(const struct qspi_instance *instance)
{
	struct qspi *spi = find_available_spi(instance->instance_number);
	if (!spi) {
		QLOGW("No more available instances. Check NRFX_SPIM_ENABLED_COUNT/NRFX_SPIS_ENABLED_COUNT.");
		return NULL;
	}

	spi->instance = instance->instance_number;
	spi->is_used = true;

	return spi;
}

static enum qerr spi_config_master(struct qspi *const spi, const struct qspi_config *config)
{
	nrfx_err_t r = NRFX_ERROR_INVALID_PARAM;

	switch (spi->instance) {
#if NRFX_SPIM_ENABLED
#if NRFX_SPIM0_ENABLED
	case 0:
		spi->type.spim.drv_inst_idx = NRFX_SPIM0_INST_IDX;
		spi->type.spim.p_reg = NRF_SPIM0;
		break;
#endif
#if NRFX_SPIM1_ENABLED
	case 1:
		spi->type.spim.drv_inst_idx = NRFX_SPIM1_INST_IDX;
		spi->type.spim.p_reg = NRF_SPIM1;
		break;
#endif
#if NRFX_SPIM2_ENABLED
	case 2:
		spi->type.spim.drv_inst_idx = NRFX_SPIM2_INST_IDX;
		spi->type.spim.p_reg = NRF_SPIM2;
		break;
#endif
#if NRFX_SPIM3_ENABLED
	case 3:
		spi->type.spim.drv_inst_idx = NRFX_SPIM3_INST_IDX;
		spi->type.spim.p_reg = NRF_SPIM3;
		break;
#endif
#else /* NRFX_SPIM_ENABLED */
#if NRFX_SPI0_ENABLED
	case 0:
		spi->type.spi.drv_inst_idx = NRFX_SPI0_INST_IDX;
		spi->type.spi.p_reg = NRF_SPI0;
		break;
#endif
#if NRFX_SPI1_ENABLED
	case 1:
		spi->type.spi.drv_inst_idx = NRFX_SPI1_INST_IDX;
		spi->type.spi.p_reg = NRF_SPI1;
		break;
#endif
#if NRFX_SPI2_ENABLED
	case 2:
		spi->type.spi.drv_inst_idx = NRFX_SPI2_INST_IDX;
		spi->type.spi.p_reg = NRF_SPI2;
		break;
#endif
#endif /* NRFX_SPIM_ENABLED */
	default:
		QLOGW("Not recognized instance.");
		return QERR_EINVAL;
	}

	memset(&spi->config, 0, sizeof(qspi_nrf_spi_config_t));

	spi->config.master.sck_pin =
		NRF_GPIO_PIN_MAP(config->sck_pin.port, config->sck_pin.pin_number);
	spi->config.master.ss_pin =
		NRF_GPIO_PIN_MAP(config->cs_pin.port, config->cs_pin.pin_number);
	spi->config.master.mosi_pin =
		NRF_GPIO_PIN_MAP(config->mosi_pin.port, config->mosi_pin.pin_number);
	spi->config.master.miso_pin =
		NRF_GPIO_PIN_MAP(config->miso_pin.port, config->miso_pin.pin_number);
	spi->config.master.irq_priority = config->irq_priority;

	spi->config.master.orc = QSPI_DEFAULT_ORC;
	spi->config.master.frequency = qspi_freq_to_nrf(config->freq_hz);
	spi->config.master.mode = qspi_master_mode_to_nrf(config->op_flags);
#if NRFX_SPIM_ENABLED
	spi->config.master.bit_order = (config->op_flags & QSPI_LSB_FIRST) ?
					       NRF_SPIM_BIT_ORDER_LSB_FIRST :
					       NRF_SPIM_BIT_ORDER_MSB_FIRST;
	spi->config.master.ss_active_high = (config->op_flags & QSPI_CS_ACTIVE_HIGH) ? true : false;

	r = nrfx_spim_init(&spi->type.spim, &spi->config.master, qspi_generic_handler_master, spi);
#else
	spi->config.master.bit_order = (config->op_flags & QSPI_LSB_FIRST) ?
					       NRF_SPI_BIT_ORDER_MSB_FIRST :
					       NRF_SPI_BIT_ORDER_LSB_FIRST;

	spi->config.master.ss_pin =
		NRF_GPIO_PIN_MAP(config->cs_pin.port, config->cs_pin.pin_number);
	spi->ss_active_high = (config->op_flags & QSPI_CS_ACTIVE_HIGH) ? true : false;

	if (spi->ss_active_high) {
		qgpio_pin_write(&config->cs_pin, 0);
	} else {
		qgpio_pin_write(&config->cs_pin, 1);
	}

	result = nrfx_spi_init(&spi->type.spi, &spi->config.master, qspi_generic_handler_master,
			       spi);
#endif /* NRFX_SPIM_ENABLED */

	if (r) {
		QLOGW("Failed to configure for master QSPI");
		return QERR_EIO;
	}

	return QERR_SUCCESS;
}

#if NRFX_SPIS_ENABLED
static enum qerr spi_config_slave(struct qspi *spi, struct qspi_config *config)
{
	nrfx_err_t r = NRFX_ERROR_INVALID_PARAM;

	spi->type.spis.drv_inst_idx = spi->instance;
	switch (spi->instance) {
#if NRFX_SPIS0_ENABLED
	case NRFX_SPIS0_INST_IDX:
		spi->type.spis.p_reg = NRF_SPIS0;
		break;
#endif
#if NRFX_SPIS1_ENABLED
	case NRFX_SPIS1_INST_IDX:
		spi->type.spis.p_reg = NRF_SPIS1;
		break;
#endif
#if NRFX_SPIS2_ENABLED
	case NRFX_SPIS2_INST_IDX:
		spi->type.spis.p_reg = NRF_SPIS2;
		break;
#endif
#if NRFX_SPIS3_ENABLED
	case NRFX_SPIS3_INST_IDX:
		spi->type.spis.p_reg = NRF_SPIS3;
		break;
#endif
	default:
		QLOGW("Not recognized instance.");
		return QERR_EINVAL;
	}

	memset(&spi->config, 0, sizeof(qspi_nrf_spi_config_t));

	spi->config.slave.sck_pin =
		NRF_GPIO_PIN_MAP(config->sck_pin.port, config->sck_pin.pin_number);
	spi->config.slave.csn_pin =
		NRF_GPIO_PIN_MAP(config->cs_pin.port, config->cs_pin.pin_number);
	spi->config.slave.mosi_pin =
		NRF_GPIO_PIN_MAP(config->mosi_pin.port, config->mosi_pin.pin_number);
	spi->config.slave.miso_pin =
		NRF_GPIO_PIN_MAP(config->miso_pin.port, config->miso_pin.pin_number);
	spi->config.slave.irq_priority = config->irq_priority;
	spi->config.slave.orc = QSPI_DEFAULT_ORC;
	spi->config.slave.mode = qspi_slave_mode_to_nrf(config->op_flags);
	spi->config.slave.bit_order = (config->op_flags & QSPI_LSB_FIRST) ?
					      NRF_SPIS_BIT_ORDER_MSB_FIRST :
					      NRF_SPIS_BIT_ORDER_LSB_FIRST;

	r = nrfx_spis_init(&spi->type.spis, &spi->config.slave, qspi_generic_handler_slave, spi);

	if (r) {
		QLOGW("Failed to configure for slave QSPI");
		return QERR_EIO;
	}

	return QERR_SUCCESS;
}
#endif /* NRFX_SPIS_ENABLED */

enum qerr qspi_configure(struct qspi *spi, const struct qspi_config *config)
{
	enum qerr ret = QERR_EINVAL;

	if (!spi || !config)
		return QERR_EINVAL;

	if (spi->is_configured)
		spi_deconfig(spi);

	spi->is_master = (QSPI_GET_ROLE(config->op_flags) == QSPI_MASTER);

	if (spi->is_master) {
		ret = spi_config_master(spi, config);
	}
#if NRFX_SPIS_ENABLED
	else {
		ret = spi_config_slave(spi, config);
	}
#endif /* NRFX_SPIS_ENABLED */

	if (ret) {
		QLOGW("Failed to configure QSPI");
		return ret;
	}

	spi->is_configured = true;

	return QERR_SUCCESS;
}

enum qerr qspi_close(struct qspi *spi)
{
	if (!spi)
		return QERR_EINVAL;

	spi_deconfig(spi);

	memset(spi, 0, sizeof(struct qspi));

	return QERR_SUCCESS;
}

enum qerr qspi_irq_set_callback(struct qspi *spi, qspi_xfer_cb handler, void *arg)
{
	if (!spi)
		return QERR_EINVAL;

	spi->handler = handler;
	spi->arg = arg;

	return QERR_SUCCESS;
}

enum qerr qspi_transceive(struct qspi *spi, const struct qspi_transfer *xfer)
{
	nrfx_err_t result = NRFX_ERROR_INVALID_PARAM;

	if (!spi)
		return QERR_EINVAL;

	spi->txf_is_finished = false;

	if (spi->is_master) {
#if NRFX_SPIM_ENABLED
		nrf_spim_enable(spi->type.spim.p_reg);

		const nrfx_spim_xfer_desc_t xfer_desc = { xfer->tx_buf, xfer->tx_size, xfer->rx_buf,
							  xfer->rx_size };
		result = nrfx_spim_xfer(&spi->type.spim, &xfer_desc, xfer->flags);
#else
		nrf_spi_enable(spi->type.spi.p_reg);

		if (spi->ss_active_high) {
			qgpio_pin_write(spi->cs_pin, 1);
		} else {
			qgpio_pin_write(spi->cs_pin, 0);
		}

		const nrfx_spi_xfer_desc_t xfer_desc = { xfer->tx_buf, xfer->tx_size, xfer->rx_buf,
							 xfer->rx_size };
		result = nrfx_spi_xfer(&spi->type.spi, &xfer_desc, 0);
#endif /* NRFX_SPIM_ENABLED */

		if (!spi->handler) {
			while (spi->txf_is_finished == false)
				;

#if NRFX_SPIM_ENABLED
			nrf_spim_disable(spi->type.spim.p_reg);
#else
			if (spi->ss_active_high) {
				qgpio_pin_write(spi->cs_pin, 0);
			} else {
				qgpio_pin_write(spi->cs_pin, 1);
			}

			nrf_spi_disable(spi->type.spi.p_reg);
#endif
		}
	}
#if NRFX_SPIS_ENABLED
	else {
		nrf_spis_enable(spi->type.spis.p_reg);

		memset(xfer->rx_buf, 0, xfer->rx_size);

		result = nrfx_spis_buffers_set(&spi->type.spis, xfer->tx_buf, xfer->tx_size,
					       xfer->rx_buf, xfer->rx_size);

		if (!spi->handler) {
			while (!spi->txf_is_finished)
				;

			nrf_spis_disable(spi->type.spis.p_reg);
		}
	}
#endif /* NRFX_SPIS_ENABLED */

	if (result != NRFX_SUCCESS)
		return QERR_EIO;

	return QERR_SUCCESS;
}
