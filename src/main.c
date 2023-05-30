/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief WiFi scan sample
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(scan, CONFIG_LOG_DEFAULT_LEVEL);


#include <nrfx_clock.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/init.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>

#include "net_private.h"

#define WIFI_SHELL_MODULE "wifi"

// #define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE)
#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_SCAN_DONE |		\
								NET_EVENT_WIFI_RAW_SCAN_RESULT)

#define NRF_LOG_DEFERRED 0

static uint32_t scan_finished;

static struct net_mgmt_event_callback wifi_shell_mgmt_cb;

static int wifi_freq_to_channel(int frequency)
{
	int channel = 0;

	if ((frequency <= 2424) && (frequency >= 2401)) {
		channel = 1;
	} else if ((frequency <= 2453) && (frequency >= 2425)) {
		channel = 6;
	} else if ((frequency <= 2484) && (frequency >= 2454)) {
		channel = 11;
	} else if ((frequency <= 5320) && (frequency >= 5180)) {
		channel = ((frequency - 5180) / 5) + 36;
	} else if ((frequency <= 5720) && (frequency >= 5500)) {
		channel = ((frequency - 5500) / 5) + 100;
	} else if ((frequency <= 5895) && (frequency >= 5745)) {
		channel = ((frequency - 5745) / 5) + 149;
	} else {
		channel = frequency;
	}

	return channel;
}

static enum wifi_frequency_bands wifi_freq_to_band(int frequency)
{
	enum wifi_frequency_bands band = WIFI_FREQ_BAND_2_4_GHZ;

	if ((frequency  >= 2401) && (frequency <= 2495)) {
		band = WIFI_FREQ_BAND_2_4_GHZ;
	} else if ((frequency  >= 5170) && (frequency <= 5895)) {
		band = WIFI_FREQ_BAND_5_GHZ;
	} else {
		band = WIFI_FREQ_BAND_6_GHZ;
	}

	return band;
}

static void handle_wifi_raw_scan_result(struct net_mgmt_event_callback *cb)
{
	struct wifi_raw_scan_result *raw =
		(struct wifi_raw_scan_result *)cb->info;
	int channel;
	int band;
	int rssi;
	uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];

	rssi = raw->rssi;
	channel = wifi_freq_to_channel(raw->frequency);
	band = wifi_freq_to_band(raw->frequency);

	// RSSI FILTER FOR TESTING (-30 should be detect the 2 devices if they are right next to each other, and not detect anything else)
	if (rssi > -128) {
		LOG_INF("%-4u (%-6s) | %-4d | %s |      %-4d        ",
			channel,
			wifi_band_txt(band),
			rssi,
			net_sprint_ll_addr_buf(raw->data + 10, WIFI_MAC_ADDR_LEN, mac_string_buf, sizeof(mac_string_buf)), raw->frame_length);

		// k_sleep(K_SECONDS(0.01));
		// LOG_HEXDUMP_INF(raw->data, sizeof(raw->data) - 128, "");  // Purposely printing less data to avoid messages dropped
		// printk("\n\n");
	}

	
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_ERR("Scan request failed (%d)", status->status);
	} else {
		scan_finished = 1;  // set global variable to indicate scanning is not longer in progress
		// LOG_INF("Scan request done");
	}
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				     uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_RAW_SCAN_RESULT:
		handle_wifi_raw_scan_result(cb);  // this func call is basically instantaneous
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		handle_wifi_scan_done(cb);  // this func call is basically instantaneous
		break;
	default:
		break;
	}
}

static int wifi_scan(void)
{
	scan_finished = 0;  // set global variable to indicate scanning currently in progresss
	
	struct net_if *iface = net_if_get_default();

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)) {
		LOG_ERR("Scan request failed");

		return -ENOEXEC;
	}

	// LOG_INF("Scan requested");

	return 0;
}

int main(void)
{
	LOG_INF("==================================PROGRAM STARTING==================================");

	net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
				     wifi_mgmt_event_handler,
				     WIFI_SHELL_MGMT_EVENTS);

	net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);

#ifdef CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT
	/* For now hardcode to 128MHz */
	nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK,
			       NRF_CLOCK_HFCLK_DIV_1);
#endif
	LOG_INF("Starting %s with CPU frequency: %d MHz", CONFIG_BOARD, SystemCoreClock / MHZ(1));  // should be nrf7002dk_nrf5340_cpuapp with 128 MHz

	scan_finished = 1;

	while(1) {
		if (scan_finished == 1) {  // only perform a scan if not another scan is currently in progress
			wifi_scan();
		}
		k_sleep(K_SECONDS(0.01));
	}

	LOG_INF("EXITED LOOP");
	
	return 0;
}
