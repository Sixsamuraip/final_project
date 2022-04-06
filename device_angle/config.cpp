#include "config.h"
#include "card-utils.h"
#include "debug.h"

/*******************************
 * Constructor
 */
Config::Config() {
  radio_device_address   = DEFAULT_RADIO_DEVICE_ADDRESS;
  radio_gateway_address  = DEFAULT_RADIO_GATEWAY_ADDRESS;
  radio_freq             = DEFAULT_RADIO_FREQ;
  radio_tx_power         = DEFAULT_RADIO_TX_POWER;
  collect_interval_day   = DEFAULT_COLLECT_INTERVAL_DAY;
  collect_interval_night = DEFAULT_COLLECT_INTERVAL_NIGHT;
  day_start_hour         = DEFAULT_DAY_START_HOUR;
  day_end_hour           = DEFAULT_DAY_END_HOUR;
  time_zone              = DEFAULT_TIME_ZONE;
  advertise_interval     = DEFAULT_ADVERTISE_INTERVAL;
  use_ack                = DEFAULT_USE_ACK;
  ack_timeout            = DEFAULT_ACK_TIMEOUT;
  long_range             = DEFAULT_LONG_RANGE;
  tx_repeat              = DEFAULT_TX_REPEAT;
  gps_max_wait_for_fix   = DEFAULT_GPS_MAX_WAIT_FOR_FIX;
  next_collect_no_fix    = DEFAULT_NEXT_COLLECT_NO_FIX;
  total_slots            = DEFAULT_TOTAL_SLOTS;
  slot_interval          = DEFAULT_SLOT_INTERVAL;
  prog_file_name[0]      = '\0';
}

/*******************************
 *
 */
static char* read_line(SdFile& file) {
  static char buf[128];
  uint8_t pos = 0;

  while (true) {
    int c = file.read();
    if (c == -1)
      if (pos == 0)
        return NULL;
      else
        break;
    if (c == '\n' || c=='\r') break;
    buf[pos++] = c;
    if (pos >= sizeof(buf)) break;
  }
  buf[pos] = '\0';
  return buf;
}

/*******************************
 *
 */
bool Config::read_from_volume(SdVolume& volume) {
  char name[100];
  char value[50];

  SdFile config_file;
  if (!open_file_in_root(config_file,volume,CONFIG_FILE_NAME,O_READ)) {
    DEBUG_PRINTLN("Cannot open config file");
    return false;
  }
  DEBUG_PRINTLN("Open config file successful");

  while (true) {
    char* s = read_line(config_file);
    if (s == NULL) break;
    sscanf(s,"%s %s",name,value);
    if (name[0] == '#') continue;
    if (strcmp(name,"radio_device_address") == 0) {
      radio_device_address = atoi(value);
    }
    else if (strcmp(name,"radio_gateway_address") == 0) {
      radio_gateway_address = atoi(value);
    }
    else if (strcmp(name,"radio_freq") == 0) {
      radio_freq = atof(value);
    }
    else if (strcmp(name,"radio_tx_power") == 0) {
      radio_tx_power = atoi(value);
    }
    else if (strcmp(name,"collect_interval_day") == 0) {
      collect_interval_day = atoi(value);
    }
    else if (strcmp(name,"collect_interval_night") == 0) {
      collect_interval_night = atoi(value);
    }
    else if (strcmp(name,"day_start_hour") == 0) {
      day_start_hour = atoi(value);
    }
    else if (strcmp(name,"day_end_hour") == 0) {
      day_end_hour = atoi(value);
    }
    else if (strcmp(name,"time_zone") == 0) {
      time_zone = atoi(value);
    }
    else if (strcmp(name,"advertise_interval") == 0) {
      advertise_interval = atoi(value);
    }
    else if (strcmp(name,"use_ack") == 0) {
      use_ack = atoi(value);
    }
    else if (strcmp(name,"ack_timeout") == 0) {
      ack_timeout = atoi(value);
    }
    else if (strcmp(name,"long_range") == 0) {
      long_range = atoi(value);
    }
    else if (strcmp(name,"tx_repeat") == 0) {
      tx_repeat = atoi(value);
    }
    else if (strcmp(name,"gps_max_wait_for_fix") == 0) {
      gps_max_wait_for_fix = atoi(value);
    }
    else if (strcmp(name,"next_collect_no_fix") == 0) {
      next_collect_no_fix = atoi(value);
    }
    else if (strcmp(name,"total_slots") == 0) {
      total_slots = atoi(value);
    }
    else if (strcmp(name,"slot_interval") == 0) {
      slot_interval = atoi(value);
    }
    else if (strcmp(name,"prog_file_name") == 0) {
      strcpy(prog_file_name,value);
    }
    else {
      DEBUG_PRINTF("Unknown parameter: %s\r\n", name);
    }
  }

  config_file.close();
  return true;
}
