#ifndef __GPS_H__
#define __GPS_H__

#include <Stream.h>

#define PMTK_SET_NMEA_OUTPUT_RMCGGA "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
#define PMTK_SET_NMEA_UPDATE_1HZ    "$PMTK220,1000*1F"
#define MAX_NMEA_LENGTH  100

class GPSClass {
public:
  GPSClass(Stream& serial);
  void begin(uint32_t baud); 
  void sendCommand(const char *);
  bool read();
  void reset_buffer(bool flush_serial);

  uint8_t hour, minute, seconds, year, month, day;
  int32_t latitude, longitude; // unit of 1/100000 degrees
  uint8_t fixquality, satellites;

  uint8_t _nmea_buf[MAX_NMEA_LENGTH];
private:
  void _parse();
  Stream& _serial;
  bool _gga_received, _rmc_received;
  uint8_t _nmea_ptr;
};

#endif
