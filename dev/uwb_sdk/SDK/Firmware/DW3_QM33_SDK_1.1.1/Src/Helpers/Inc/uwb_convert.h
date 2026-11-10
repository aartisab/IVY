/**
 * @file      uwb_convert.c
 *
 * @brief     Set of function used to convert angles.
 *
 * @author    Qorvo Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo US, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#pragma once

#include <stdio.h>

float convert_aoa_2pi_q16_to_deg(int16_t aoa_2pi_q16);
float convert_rssi_q7_to_dbm(uint8_t rssi_q7);
