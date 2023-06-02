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

// decimal of FA 0B BC 0D
static uint8_t identifier[] = {250, 11, 188, 13};

// array of ASCII chars, where their index in th array corresponds to its decimal representation.
// note that the non-alphanumeric and non-null values are not needed and so are left as underscores.
static char ASCII_DICTIONARY[] = {'0', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_', '_', '_', '_', '_', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};

static uint32_t scan_finished;



enum ID_TYPE {
	ID_NONE = 0, 
	SERIAL_NUMBER_ANSI_CTA_2063_A = 1, 
	CAA_ASSIGNED_REGISTRATION_ID = 2, 
	UTM_ASSIGND_UUID = 3, 
	SPECIFIC_SESSION_ID = 4
};
static const char* const ID_TYPE_STRING[] = {
	[ID_NONE] = "ID_NONE",
	[SERIAL_NUMBER_ANSI_CTA_2063_A] = "SERIAL_NUMBER_ANSI_CTA_2063_A",
	[CAA_ASSIGNED_REGISTRATION_ID] = "CAA_ASSIGNED_REGISTRATION_ID",
	[UTM_ASSIGND_UUID] = "UTM_ASSIGND_UUID",
	[SPECIFIC_SESSION_ID] = "SPECIFIC_SESSION_ID"
};

enum UA_TYPE {
	UA_NONE = 0, 
	AEROPLANE = 1, 
	HELICOPTER_MULTIROTOR = 2, 
	GYROPLANE = 3, 
	HYBRID_LIFT = 4, 
	ORNITHOPTOR = 5, 
	GLIDER = 6, 
	KITE = 7, 
	FREE_BALLOON = 8, 
	CAPTIVE_BALLOON = 9, 
	AIRSHIP = 10, 
	FREE_FALL_PARACHUTE = 11, 
	ROCKET = 12, 
	TETHERED_POWERED_AIRCRAFT = 13, 
	GROUND_OBSTACLE = 14, 
	OTHER = 15
};
static const char* const UA_TYPE_STRING[] = {
	[UA_NONE] = "UA_NONE",
	[AEROPLANE] = "AEROPLANE",
	[HELICOPTER_MULTIROTOR] = "HELICOPTER_MULTIROTOR",
	[GYROPLANE] = "GYROPLANE",
	[HYBRID_LIFT] = "HYBRID_LIFT",
	[ORNITHOPTOR] = "ORNITHOPTOR",
	[GLIDER] = "GLIDER",
	[KITE] = "KITE",
	[FREE_BALLOON] = "FREE_BALLOON",
	[CAPTIVE_BALLOON] = "CAPTIVE_BALLOON",
	[AIRSHIP] = "AIRSHIP",
	[FREE_FALL_PARACHUTE] = "FREE_FALL_PARACHUTE",
	[ROCKET] = "ROCKET",
	[TETHERED_POWERED_AIRCRAFT] = "TETHERED_POWERED_AIRCRAFT",
	[GROUND_OBSTACLE] = "GROUND_OBSTACLE",
	[OTHER] = "OTHER"
};

enum OPERATIONAL_STATUS {
	UNDECLARED = 0, 
	GROUND = 1, 
	AIRBORNE = 2, 
	EMERGENCY = 3, 
	REMOTE_ID_SYSTEM_FAILURE = 4
};
static const char* const OPERATIONAL_STATUS_STRING[] = {
	[UNDECLARED] = "UNDECLARED",
	[GROUND] = "GROUND",
	[AIRBORNE] = "AIRBORNE",
	[EMERGENCY] = "EMERGENCY",
	[REMOTE_ID_SYSTEM_FAILURE] = "REMOTE_ID_SYSTEM_FAILURE"
};


enum HEIGHT_TYPE {
	ABOVE_TAKEOFF = 0,
	AGL = 1
};
static const char* const HEIGHT_TYPE_STRING[] = {
	[ABOVE_TAKEOFF] = "ABOVE_TAKEOFF",
	[AGL] = "AGL"  // Above Ground Level
};

enum E_W_DIRECTION_SEGMENT {
	LESS_THAN_180 = 0,
	GREATER_THAN_EQUAL_TO_180 = 1
};
static const char* const E_W_DIRECTION_SEGMENT_STRING[] = {
	[LESS_THAN_180] = "<180",
	[GREATER_THAN_EQUAL_TO_180] = ">=180"
};

enum SPEED_MULTIPLIER {
	X_0_25 = 0,
	X_0_75 = 1
};
static const char* const SPEED_MULTIPLIER_STRING[] = {
	[X_0_25] = "0.25",
	[X_0_75] = "0.75"
};

enum VERTICAL_HORIZONTAL_ACCURACY {
	UNKNOWN_GREATER_THAN_150M = 0, 
	LESS_THAN_150M = 1, 
	LESS_THAN_45M = 2, 
	LESS_THAN_25M = 3, 
	LESS_THAN_10M = 4,
	LESS_THAN_3M = 5,
	LESS_THAN_1M = 6,
};
static const char* const VERTICAL_HORIZONTAL_ACCURACY_STRING[] = {
	[UNKNOWN_GREATER_THAN_150M] = "UNKNOWN OR >=150 m",
	[LESS_THAN_150M] = "<150 m",
	[LESS_THAN_45M] = "<45 m",
	[LESS_THAN_25M] = "<25 m",
	[LESS_THAN_10M] = "<10 m",
	[LESS_THAN_3M] = "<3 m",
	[LESS_THAN_1M] = "<1 m"
};




static struct net_mgmt_event_callback wifi_shell_mgmt_cb;

static void log_hexdump(uint8_t* buf, uint16_t size) {
	/*
	 print the buffer (which should be a scanned wifi packet) as a string of hex chars.
	 */
	for (int i=0; i<size; i++) {
		if (buf[i] < 16) {  // because printing "%x ", for single digit characters omits the first 0
			printk("0");
		}
		printk("%X ", buf[i]);
		k_sleep(K_SECONDS(0.001));  // delay between each character so messages aren't dropped
	}
	printk("\n");
}

static int contains(uint8_t big[], int size_b, uint8_t small[], int size_s) {
	/*
	 Checks if a small array is a sub-array of a big array. Returns -1 if it's not,
	 Returns the index of the small array in the big array if it is.
	 */
    int contains, k;
    for(int i = 0; i < size_b; i++){
        // assume that the big array contains the small array
        contains = 1;
        // check if the element at index i in the big array is the same as the first element in the small array
       if(big[i] == small[0]){
           // if yes, then we start from k = 1, because we already know that the first element in the small array is the same as the element at index i in the big array
           k = 1;
           // we start to check if the next elements in the big array are the same as the elements in the small array
           // (we start from i+1 position because we already know that the element at the position i is the same as the first element in the small array)
           for(int j = i + 1; j < size_b; j++){
               // range for k must be from 1 to size_s-1
               if(k >= size_s - 1) {
                   break;
               }
               // if the element at the position j in the big array is different
               // from the element at the position k in the small array then we
               // flag that we did not find the sequence we were looking for (contains=0)
               if(big[j] != small[k]){
                   contains = 0;
                   break;
               }
               // increment k because we want the next element in the small array
               k++;
           }
           // if contains flag is not 0 that means we found the sequence we were looking
           // for and that sequence starts from index i in the big array
           if(contains) {
               return i;
           }
       }
    }
    return -1;  // if the sequence we were looking for was not found
}

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

	int odid_identifier_idx = contains(raw->data, sizeof(raw->data), identifier, sizeof(identifier));

	// if (odid_identifier_idx != -1) {
	if (rssi > -30) {
		LOG_INF("%-4u (%-6s) | %-4d | %s |      %-4d        ",
			channel,
			wifi_band_txt(band),
			rssi,
			net_sprint_ll_addr_buf(raw->data + 10, WIFI_MAC_ADDR_LEN, mac_string_buf, sizeof(mac_string_buf)), raw->frame_length);

		k_sleep(K_SECONDS(0.01));
		log_hexdump(raw->data, sizeof(raw->data));
		printk("\n\n");

		int num_msg_in_pack = raw->data[odid_identifier_idx+7];
		for (int msg_num=0; msg_num<num_msg_in_pack; msg_num++) {
			int msg_type = (raw->data[odid_identifier_idx + 8 + msg_num*25] - 2) / 16;  // either 0 (Basic ID), 1 (Location/Vector), 2 (Authentication), 3 (Self-ID), 4 (System), or 5 (Operator ID)
			
			switch(msg_type) {  // Decode the raw data into useful RID information
				case 0:  // Basic ID Message
					enum UA_TYPE ua_type = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] % 16;
					enum ID_TYPE id_type = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] / 16;  // floor division
					
					// Loop through the Serial number in the raw decimal data and build a new string containing the serial number in ASCII
					char id_buf[21];  // initalize char array to hold the serial number (which is at most 20 ASCII chars)
					for (int i=0; i<20; i++) {
						int decimal_val = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2+i];
						id_buf[i] = ASCII_DICTIONARY[decimal_val];
					}
					id_buf[20] = "\0";
					strcat(id_buf, "\0");  // add null terminator for printing
					printf("\n\nID TYPE: %s.  ", ID_TYPE_STRING[id_type]);
					printf("UA TYPE: %s.  ", UA_TYPE_STRING[ua_type]);
					printf("SERIAL NUMBER/UAV ID: %s.\n\n", id_buf);
				case 1:  // Location/Vector Message
					enum OPERATIONAL_STATUS op_status = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] / 16;  // floor division
					// funky modulos & floor division to get the value of each bit
					enum HEIGHT_TYPE height_type_flag = (raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] % 8) / 4;  // 0: Above Takeoff. 1: AGL
					enum E_W_DIRECTION_SEGMENT direction_segment_flag = (raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] % 4) / 2;  // 0: <180, 1: >=180
					enum SPEED_MULTIPLIER speed_multiplier_flag = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] % 2;;  // 0: x0.25, 1: x0.75

					uint8_t track_direction = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2];  // direction measured clockwise from true north. Should be between 0-179
					if (direction_segment_flag) {  // If E_W_DIRECTION_SEGMENT is true, add 180 to the track_direction, according to ASTM.
						track_direction += 180;
					}

					uint8_t speed = raw->data[odid_identifier_idx + 8 + msg_num*25 + 3];  // ground speed in m/s
					if (speed_multiplier_flag) {
						speed = 0.75*speed + 255*0.25;  // as defined in ASTM
					}
					else {
						speed *= 0.25;
					}

					int8_t vertical_speed = raw->data[odid_identifier_idx + 8 + msg_num*25 + 4] * 0.5;  // vertical speed in m/s (positive = up, negatve = down); multiply by 0.5 as defined in ASTM

					// TODO: fix lat and lon
					double lat = raw->data[odid_identifier_idx + 8 + msg_num*25 + 5];

					double lon = raw->data[odid_identifier_idx + 8 + msg_num*25 + 9];

					// TODO: fix altitudes
					double pressure_altitude = (raw->data[odid_identifier_idx + 8 + msg_num*25 + 13] * 0.5) - 1000;

					double geodetic_altitude = (raw->data[odid_identifier_idx + 8 + msg_num*25 + 15] * 0.5) - 1000;

					// TODO: fix height
					double height = (raw->data[odid_identifier_idx + 8 + msg_num*25 + 17] * 0.5) - 1000;

					enum VERTICAL_HORIZONTAL_ACCURACY vertical_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 19] / 16;  // floor division
					enum VERTICAL_HORIZONTAL_ACCURACY horizontal_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 19] % 16;

					// TODO: fix timestamp
					double timestamp = raw->data[odid_identifier_idx + 8 + msg_num*25 + 20];  // 1/10ths of seconds since the last hour relatve to UTC time

					uint8_t timestamp_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 22] * 0.1;  // between 0.1 s and 1.5 s (0 s = unknown)
			}
			
			printk("%d\n", msg_type);
		}
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
