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

#include "enums.h"

#define WIFI_SHELL_MODULE "wifi"

// #define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE)
#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_SCAN_DONE |		\
								NET_EVENT_WIFI_RAW_SCAN_RESULT)

#define NRF_LOG_DEFERRED 0

// decimal of FA 0B BC 0D
static uint8_t identifier[] = {250, 11, 188, 13};

// array of ASCII chars, where their index in the array corresponds to its decimal representation.
// chars that aren't needed for our purposes are left as underscores
static const char ASCII_DICTIONARY[] = {'0', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '_', '-', '.', '_', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '_', '_', '_', '_', '_', '_', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
// array of hex chars, where their index in the array corresponds to its decimal representation
static const char* const HEX_DICTIONARY[] = {"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B", "3C", "3D", "3E", "3F", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E", "4F", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F", "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF", "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF", "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF", "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF", "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF"};


static uint32_t scan_finished;


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

	if (odid_identifier_idx != -1) {
		LOG_INF("%-4u (%-6s) | %-4d | %s |      %-4d        ",
			channel,
			wifi_band_txt(band),
			rssi,
			net_sprint_ll_addr_buf(raw->data + 10, WIFI_MAC_ADDR_LEN, mac_string_buf, sizeof(mac_string_buf)), raw->frame_length);

		k_sleep(K_SECONDS(0.01));
		log_hexdump(raw->data, sizeof(raw->data));
		printk("\n\n");
		
		// flags to hold whether or not a certain message was received
		int basic_id_flag = 0;
		int location_vector_flag = 0;
		int authentication_flag = 0;
		int self_id_flag = 0;
		int system_flag = 0;
		int operator_id_flag = 0;

		int num_msg_in_pack = raw->data[odid_identifier_idx+7];
		for (int msg_num=0; msg_num<num_msg_in_pack; msg_num++) {
			int msg_type = (raw->data[odid_identifier_idx + 8 + msg_num*25] - 2) / 16;  // either 0 (Basic ID), 1 (Location/Vector), 2 (Authentication), 3 (Self-ID), 4 (System), or 5 (Operator ID)
			
			switch(msg_type) {  // Decode the raw data into useful RID information
				case 0:  // Basic ID Message
					enum UA_TYPE ua_type = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] % 16;
					enum ID_TYPE id_type = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] / 16;  // floor division
					printf("\n\nID TYPE: %s.  ", ID_TYPE_STRING[id_type]);
					printf("UA TYPE: %s.  ", UA_TYPE_STRING[ua_type]);

					switch(id_type) {
						// Using curly bracs around each case to define each case as it's own frame to prevent redeclaration errors for id_buf
						case 0:{  // None --> null ID
							char id_buf[] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '\0'};
							printf("NULL UAV ID: %s.\n\n", id_buf);
							break;
						}
						case 1:  // Serial Number
						case 2:{  // CAA Registration
							// Loop through the Serial number in the raw decimal data and build a new string containing the serial number in ASCII
							char id_buf[21];  // initalize char array to hold the serial number (which is at most 20 ASCII chars)
							for (int i=0; i<20; i++) {
								int decimal_val = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2+i];
								id_buf[i] = ASCII_DICTIONARY[decimal_val];
							}
							id_buf[20] = '\0';
							printf("SERIAL NUMBER/CAA REGISTRATION NUMBER: %s.\n\n", id_buf);
							break;
						}
						case 3:{  // UTM UUID (should be encoded as a 128-bit UUID (32-char hex string))
							char id_buf[33];
							for (int i=0; i<32; i++) {
								int decimal_val = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2+i];
								id_buf[i] = HEX_DICTIONARY[decimal_val][0];  // 1st hex char in an array of 2 hex chars
								id_buf[i+1] = HEX_DICTIONARY[decimal_val][1];  // 2nd hex char in an array of 2 hex chars
							}
							id_buf[32] = '\0';
							printf("UTM UUID: %s.\n\n", id_buf);
							break;
						}							
						case 4:{  // Specific Session ID (1st byte is an in betwen 0 and 255, and 19 remaining bytes are alphanumeric code, according to this: https://www.rfc-editor.org/rfc/rfc9153.pdf)
							char* id_buf[21];  // initalize char array to hold the serial number (which is at most 20 ASCII chars)
							sprintf(id_buf[0], "%d", raw->data[odid_identifier_idx + 8 + msg_num*25 + 2]);
							// Loop through the Session ID in the raw decimal data and build a new string containing the serial number in ASCII
							for (int i=1; i<20; i++) {
								int decimal_val = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2+i];
								id_buf[i] = ASCII_DICTIONARY[decimal_val];
							}
							// printf("SPECIFIC SESSION ID: %s.\n\n", id_buf);
							break;
						}
					}
					basic_id_flag = 1;
					break;
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

					double vertical_speed = raw->data[odid_identifier_idx + 8 + msg_num*25 + 4] * 0.5;  // vertical speed in m/s (positive = up, negatve = down); multiply by 0.5 as defined in ASTM

					// lat and lon Little Endian encoded
					int32_t lat_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 5];
					int32_t lat_lsb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 6] * (1<<8);  // multiply by 2^8
					int32_t lat_msb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 7] * (1<<16);  // multiply by 2^16
					int32_t lat_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 8] * (1<<24);  // multiply by 2^24
					double lat = lat_msb + lat_msb1 + lat_lsb1 + lat_lsb;

					int32_t lon_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 9];
					int32_t lon_lsb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 10] * (1<<8);  // multiply by 2^8
					int32_t lon_msb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 11] * (1<<16);  // multiply by 2^16
					int32_t lon_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 12] * (1<<24);  // multiply by 2^24
					double lon = lon_msb + lon_msb1 + lon_lsb1 + lon_lsb / 10000000;  // divide by 10^7, according to ASTM

					// altitudes and height Little Endian encoded
					uint16_t pressure_altitude_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 13];
					uint16_t pressure_altitude_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 14] * (1<<8);
					uint16_t pressure_altitude = (pressure_altitude_msb + pressure_altitude_lsb) * 0.5 - 1000;

					uint16_t geodetic_altitude_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 15];
					uint16_t geodetic_altitude_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 16] * (1<<8);
					uint16_t geodetic_altitude = (pressure_altitude_msb + pressure_altitude_lsb) * 0.5 - 1000;

					uint16_t height_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 17];
					uint16_t height_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 18] * (1<<8);
					uint16_t height = (pressure_altitude_msb + pressure_altitude_lsb) * 0.5 - 1000;

					enum VERTICAL_HORIZONTAL_BARO_ALT_ACCURACY vertical_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 19] / 16;  // floor division
					enum VERTICAL_HORIZONTAL_BARO_ALT_ACCURACY horizontal_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 19] % 16;
					
					enum VERTICAL_HORIZONTAL_BARO_ALT_ACCURACY baro_alt_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 20] / 16;  // floor division
					enum SPEED_ACCURACY speed_accuracy = raw->data[odid_identifier_idx + 8 + msg_num*25 + 20] % 16;

					// 1/10ths of seconds since the last hour relatve to UTC time
					uint16_t timestamp_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 21];
					uint16_t timestamp_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 21] * (1<<8);
					uint16_t timestamp = timestamp_msb + timestamp_lsb;

					uint8_t timestamp_accuracy = (raw->data[odid_identifier_idx + 8 + msg_num*25 + 22] % 15) * 0.1;  // modulo 15 to get rid of bits 7-4. Between 0.1 s and 1.5 s (0 s = unknown)
					
					printf("OPERATIONAL STATUS: %s.  ", OPERATIONAL_STATUS_STRING[op_status]);
					printf("HEIGHT TYPE: %s.  ", HEIGHT_TYPE_STRING[height_type_flag]);
					printf("DIRECTION SEGMENT FLAG: %s.  ", E_W_DIRECTION_SEGMENT_STRING[direction_segment_flag]);
					printf("SPEED MULTIPLIER FLAG: %s.  ", SPEED_MULTIPLIER_STRING[speed_multiplier_flag]);
					printf("HEADING (deg): %d.  ", track_direction);
					printf("SPEED (m/s): %f.  ", speed);
					printf("VERTICAL SPEED (m/s): %f", vertical_speed);
					printf("LAT: %f.  ", lat);
					printf("LON: %f.  ", lon);
					printf("PRESSURE ALT: %d.  ", pressure_altitude);
					printf("GEO ALT: %d.  ", geodetic_altitude);
					printf("HEIGHT: %d.  ", height);
					printf("HORIZONTAL ACCURACY: %s.  ", VERTICAL_HORIZONTAL_BARO_ALT_ACCURACY_STRING[horizontal_accuracy]);
					printf("VERTICAL ACCURACY: %s.  ", VERTICAL_HORIZONTAL_BARO_ALT_ACCURACY_STRING[vertical_accuracy]);
					printf("BARO ALT ACCURACY: %s.  ", VERTICAL_HORIZONTAL_BARO_ALT_ACCURACY_STRING[baro_alt_accuracy]);
					printf("SPEED ACCURACY: %s.  ", SPEED_ACCURACY_STRING[speed_accuracy]);
					printf("TIMESTAMP: %d", timestamp);
					printf("TIMESTAMP_ACCURACY (btwn 0.1-1.5s): %f\n\n", timestamp_accuracy);

					location_vector_flag = 1;
					break;
				case 3:  // Self ID Message
					enum SELF_ID_TYPE self_id_type = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1];
					// Loop through the self id description in the raw decimal data and build a new string containing the self id description in ASCII
					char self_id_description_buf[24];  // initalize char array to hold the serial number (which is at most 20 ASCII chars)
					for (int i=0; i<23; i++) {
						int decimal_val = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2 + i];
						self_id_description_buf[i] = ASCII_DICTIONARY[decimal_val];
					}
					self_id_description_buf[23] = '\0';
					printf("SELF ID: %s.\n\n", self_id_description_buf);
					self_id_flag = 1;
					break;
				case 4:  // System Message
					enum OPERATOR_LOCATION_ALTITUDE_SOURCE_TYPE operator_location_type = raw->data[odid_identifier_idx + 8 + msg_num*25 + 1] % 3; // mod 3 so that we only consider bits 1 and 0
					
					// lat and lon Little Endian encoded
					int32_t operator_lat_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2];
					int32_t operator_lat_lsb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 3] * (1<<8);  // multiply by 2^8
					int32_t operator_lat_msb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 4] * (1<<16);  // multiply by 2^16
					int32_t operator_lat_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 5] * (1<<24);  // multiply by 2^24
					double operator_lat = lat_msb + lat_msb1 + lat_lsb1 + lat_lsb;

					int32_t operator_lon_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 6];
					int32_t operator_lon_lsb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 7] * (1<<8);  // multiply by 2^8
					int32_t operator_lon_msb1 = raw->data[odid_identifier_idx + 8 + msg_num*25 + 8] * (1<<16);  // multiply by 2^16
					int32_t operator_lon_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 9] * (1<<24);  // multiply by 2^24
					double operator_lon = lon_msb + lon_msb1 + lon_lsb1 + lon_lsb / 10000000;  // divide by 10^7, according to ASTM

					// number of aircraft in the area
					uint16_t area_count_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 10];
					uint16_t area_count_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 11] * (1<<8);  // multiply by 2^8
					uint16_t area_count = area_count_msb + area_count_lsb;

					// radius of cylindrical area with the group of aircraft
					uint16_t area_radius = raw->data[odid_identifier_idx + 8 + msg_num*25 + 12] * 10;

					// floor and ceiling Little Endian encoded
					uint16_t area_ceiling_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 13];
					uint16_t area_ceiling_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 14] * (1<<8);
					uint16_t area_ceiling = (pressure_altitude_msb + pressure_altitude_lsb) * 0.5 - 1000;

					uint16_t area_floor_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 15];
					uint16_t area_floor_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 16] * (1<<8);
					uint16_t area_floor = (pressure_altitude_msb + pressure_altitude_lsb) * 0.5 - 1000;

					enum UA_CATEGORY ua_category = raw->data[odid_identifier_idx + 8 + msg_num*25 + 17] / 16;  // floor division
					if (ua_category == OPEN) {
						enum UA_CLASS ua_classification = raw->data[odid_identifier_idx + 8 + msg_num*25 + 17] % 16;
					}
					else {
						enum UA_CATEGORY ua_classification = 0;
					}

					// operator altitude Little Endian encoded
					uint16_t operator_altitude_lsb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 18];
					uint16_t operator_altitude_msb = raw->data[odid_identifier_idx + 8 + msg_num*25 + 19] * (1<<8);
					uint16_t operator_altitude = (pressure_altitude_msb + pressure_altitude_lsb) * 0.5 - 1000;

					// skipping timestamp

					printf("OPERATOR LOCATION SOURCE TYPE: %s.  ", OPERATOR_LOCATION_ALTITUDE_SOURCE_TYPE_STRING[operator_location_type]);
					printf("OPERATOR LAT: %f.  ", operator_lat);
					printf("OPERATOR LON: %f.  ", operator_lon);
					printf("OPERATOR ALT: %d.  ", operator_altitude);
					printf("AREA COUNT: %d.  ", area_count);
					printf("AREA RADIUS: %d.  ", area_radius);
					printf("AREA CEILING: %d.  ", area_ceiling);
					printf("AREA FLOOR: %d.  ", area_floor);
					printf("UA CATEGORY: %s.  ", UA_CATEGORY_STRING[ua_category]);
					printf("UA CLASS: %s.\n\n", UA_CLASS_STRING[ua_class]);
					
					system_flag = 1;
					break;
				case 5:  // Operator ID Message
					// Loop through the operator id in the raw decimal data and build a new string containing the operator id in ASCII
					char operator_id_buf[21];  // initalize char array to hold the serial number (which is at most 20 ASCII chars)
					for (int i=0; i<20; i++) {
						int decimal_val = raw->data[odid_identifier_idx + 8 + msg_num*25 + 2 + i];
						operator_id_buf[i] = ASCII_DICTIONARY[decimal_val];
					}
					operator_id_buf[20] = '\0';
					printf("OPERATOR ID (CAA-issued License): %s.\n\n", operator_id_buf);

					operator_id_flag = 1;
					break;
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
