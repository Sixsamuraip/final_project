#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <SD.h>

/**************************************************************************
 * Device's 8-bit address
 */
#ifndef DEFAULT_RADIO_DEVICE_ADDRESS
#define DEFAULT_RADIO_DEVICE_ADDRESS  0x01
#endif

/**************************************************************************
 * Gateway's 8-bit address
 */
#ifndef DEFAULT_RADIO_GATEWAY_ADDRESS
#define DEFAULT_RADIO_GATEWAY_ADDRESS   0x00
#endif

/**************************************************************************
 * Central frequency for LoRa module
 */
#ifndef DEFAULT_RADIO_FREQ
#define DEFAULT_RADIO_FREQ        450.0
#endif

/**************************************************************************
 * Transmission power in dBm
 */
#ifndef DEFAULT_RADIO_TX_POWER
#define DEFAULT_RADIO_TX_POWER    23
#endif

/**************************************************************************
 * Day-time data collection interval (in seconds)
 */
#ifndef DEFAULT_COLLECT_INTERVAL_DAY
#define DEFAULT_COLLECT_INTERVAL_DAY  600
#endif

/**************************************************************************
 * Night-time data collection interval (in seconds)
 */
#ifndef DEFAULT_COLLECT_INTERVAL_NIGHT
#define DEFAULT_COLLECT_INTERVAL_NIGHT  3600
#endif

/**************************************************************************
 * Hour (0-23) that marks the start of day
 */
#ifndef DEFAULT_DAY_START_HOUR
#define DEFAULT_DAY_START_HOUR  6
#endif

/**************************************************************************
 * Hour (0-23) that marks the end of day
 */
#ifndef DEFAULT_DAY_END_HOUR
#define DEFAULT_DAY_END_HOUR  18
#endif

/**************************************************************************
 * Time zone, measured in hours (+/-) from UTC
 */
#ifndef DEFAULT_TIME_ZONE
#define DEFAULT_TIME_ZONE  7
#endif

/**************************************************************************
 * Gateway's advertisement interval (in seconds)
 * (for ack method only)
 */
#ifndef DEFAULT_ADVERTISE_INTERVAL
#define DEFAULT_ADVERTISE_INTERVAL      30
#endif

/**************************************************************************
 * Use advertisement/ack from gateway to reliably upload data
 * Not using ack will also enable power-saving mode
 */
#ifndef DEFAULT_USE_ACK
#define DEFAULT_USE_ACK   0
#endif

/**************************************************************************
 * Time waiting for gateway before next retransmission attempt (in seconds)
 * (for ack method only)
 */
#ifndef DEFAULT_ACK_TIMEOUT
#define DEFAULT_ACK_TIMEOUT       5
#endif

/**************************************************************************
 * Long-range mode enable flag
 * 0 - high-speed, short-range
 * 1 - low-speed, long-range
 */
#ifndef DEFAULT_LONG_RANGE
#define DEFAULT_LONG_RANGE        1
#endif

/**************************************************************************
 * Number of duplicate transmissions per report
 * (for low-power method only)
 */
#ifndef DEFAULT_TX_REPEAT
#define DEFAULT_TX_REPEAT         3
#endif

/**************************************************************************
 * Amount of time to wait for GPS to acquire fix before giving up
 * (in seconds)
 */
#ifndef DEFAULT_GPS_MAX_WAIT_FOR_FIX
#define DEFAULT_GPS_MAX_WAIT_FOR_FIX  120
#endif

/**************************************************************************
 * Amount of time to wait before the next attempt after no fix was acquired
 * (in seconds)
 */
#ifndef DEFAULT_NEXT_COLLECT_NO_FIX
#define DEFAULT_NEXT_COLLECT_NO_FIX  120  // seconds
#endif

/**************************************************************************
 * Amount of time to try waiting for NMEA from the GPS module after fix has
 * been acquired (in seconds)
 */
#ifndef DEFAULT_GPS_NMEA_TIMEOUT
#define DEFAULT_GPS_NMEA_TIMEOUT  5  // seconds
#endif

/**************************************************************************
 * Number of slots
 * (for low-power method only)
 */
#ifndef DEFAULT_TOTAL_SLOTS
#define DEFAULT_TOTAL_SLOTS  40
#endif

/**************************************************************************
 * Slot interval in seconds
 * (for low-power method only)
 */
#ifndef DEFAULT_SLOT_INTERVAL
#define DEFAULT_SLOT_INTERVAL  10
#endif

/**************************************************************************
 * Partition ID on SD card for storing data
 */
#ifndef STORAGE_PARTITION_ID
#define STORAGE_PARTITION_ID   0x33
#endif

/**************************************************************************
 * Partition ID on SD card for storing logs
 */
#ifndef LOG_PARTITION_ID
#define LOG_PARTITION_ID  0x34
#endif

/**************************************************************************
 * Partition ID on SD card for configuration data
 */
#ifndef CONFIG_PARTITION_ID
#define CONFIG_PARTITION_ID   0x0E
#endif

/**************************************************************************
 * Name of the configuration file on the configuration partition
 */
#ifndef CONFIG_FILE_NAME
#define CONFIG_FILE_NAME   "CONFIG.TXT"
#endif

struct Config {
  uint8_t  radio_device_address;
  uint8_t  radio_gateway_address;
  float    radio_freq;
  uint8_t  radio_tx_power;
  uint16_t collect_interval_day;
  uint16_t collect_interval_night;
  uint8_t  day_start_hour;
  uint8_t  day_end_hour;
  int8_t   time_zone;
  uint16_t advertise_interval;
  uint8_t  use_ack;
  uint16_t ack_timeout;
  uint8_t  long_range;
  uint8_t  tx_repeat;
  uint16_t gps_max_wait_for_fix;
  uint16_t next_collect_no_fix;
  uint16_t total_slots;
  uint16_t slot_interval;
  char     prog_file_name[10];

  Config();
  bool read_from_volume(SdVolume& volume);
} __attribute__((packed));


#endif
