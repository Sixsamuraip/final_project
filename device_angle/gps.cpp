#include <Arduino.h>
#include <stdint.h>

//#define DEBUG
#include "debug.h"
#include "gps.h"

/***********************************************
 * Borrowed from Adafruit's GPS library
 */
static uint8_t parseHex(char c) {
    if (c < '0')
      return 0;
    if (c <= '9')
      return c - '0';
    if (c < 'A')
       return 0;
    if (c <= 'F')
       return (c - 'A')+10;
    // if (c > 'F')
    return 0;
}

/***********************************************
 * Extract a comma-separated field.
 * The result is kept in buf and pointer to the next field is returned
 */
static char* extract_field(char* nmea, char* buf) {
  while (*nmea) {
    *buf++ = *nmea++;
    if (*nmea == ',') {
      nmea++;
      break;
    }
  }
  *buf = 0;
  return nmea;
}

/***********************************************
 * 
 */
GPSClass::GPSClass(Stream& serial):
  _serial(serial)
{
  //reset_buffer(true);
}

/***********************************************
 * 
 */
void GPSClass::sendCommand(const char *cmd) {
  _serial.println(cmd);
}

/***********************************************
 * 
 */
static int32_t to_coord(const char* buf) {
  int32_t rawval = (int32_t)(atof(buf)*1000);
  return (rawval/100000)*100000 + (rawval%100000)*10/6;
}

/***********************************************
 * Try to parse whatever is stored in the NMEA buffer, then set _rmc_received
 * or _gga_received flags if corresponding info has been obtained
 */
void GPSClass::_parse() {
  // checksum check, obtained from Adafruit's GPS library

  // first look if we even have one
  uint16_t slen = strlen((char*)_nmea_buf);
  //DEBUG_PRINT("NMEA: "); DEBUG_PRINTLN((char*)_nmea_buf);
  if (_nmea_buf[slen-3] == '*') {
    uint16_t sum = parseHex(_nmea_buf[slen-2]) * 16;
    sum += parseHex(_nmea_buf[slen-1]);
    //DEBUG_PRINT("NMEA checksum: "); DEBUG_PRINTLN(sum);
    
    // check checksum 
    for (uint8_t i=1; i < slen-3; i++) {
      sum ^= _nmea_buf[i];
    }
    if (sum != 0) {
      // bad checksum :(
      DEBUG_PRINT("Checksum failed: "); DEBUG_PRINTLN(sum);
      return;
    }
  }
  else {
    return; // checksum not exist
  }

  char buf[20];
  _nmea_buf[2] = 'P'; // treat all sentences as $GP
  char* ptr = (char*)_nmea_buf;
  ptr = extract_field(ptr,buf);
  if (!strcmp_P(buf,PSTR("$GPGGA"))) {
    //$GPGGA,050119.000,1350.7864,N,10034.1178,E,1,08,0.96,6.5,M,-27.7,M,,*48

    // time
    ptr = extract_field(ptr,buf);
    uint32_t time = (uint32_t)atof(buf);
    hour = time/10000;
    minute = (time%10000)/100;
    seconds = (time%100);

    // latitude
    int32_t rawval;
    ptr = extract_field(ptr,buf);
    latitude = to_coord(buf);
    if (*ptr == 'S') latitude = -latitude;
    ptr = extract_field(ptr,buf);

    // longitude
    ptr = extract_field(ptr,buf);
    longitude = to_coord(buf);
    if (*ptr == 'W') longitude = -longitude;
    ptr = extract_field(ptr,buf);

    // fix quality
    ptr = extract_field(ptr,buf);
    fixquality = atoi(buf);

    // num satellites
    ptr = extract_field(ptr,buf);
    satellites = atoi(buf);

    if (fixquality) _gga_received = true;

    //Serial.print(hour);
    //Serial.print(':');
    //Serial.print(minute);
    //Serial.print(':');
    //Serial.print(seconds);
    //Serial.print(' ');
    //Serial.print(latitude);
    //Serial.print(',');
    //Serial.print(longitude);
    //Serial.print(' ');
    //Serial.print(fixquality);
    //Serial.print(',');
    //Serial.print(satellites);
    //Serial.println();
  }
  else if (!strcmp_P(buf,PSTR("$GPRMC"))) {
    //$GPRMC,050119.000,A,1350.7864,N,10034.1178,E,0.70,33.46,310519,,,A*5B
    for (uint8_t i = 0; i < 9; i++)
      ptr = extract_field(ptr,buf);
    uint32_t date = atol(buf);

    if (date) {
      day = date/10000;
      month = (date%10000)/100;
      year = date%100;
      _rmc_received = true;
      //Serial.print("20");
      //Serial.print(year);
      //Serial.print('-');
      //Serial.print(month);
      //Serial.print('-');
      //Serial.print(day);
      //Serial.println();
    }
  }
}

/***********************************************
 * Read and parse NMEA sentence, then return true when all required
 * information has been received
 */
bool GPSClass::read() {
  if (_gga_received && _rmc_received) 
    return true;

  if (!_serial.available()) {
    //DEBUG_PRINT('.');
    return false;
  }

  char c = _serial.read();
  //DEBUG_PRINT(c);

  if (c == '$') {
    // got start of NMEA sentence; start over
    _nmea_ptr = 0;
    _nmea_buf[_nmea_ptr++] = c;
    return false;
  }
  else if (c == '\n' || c == '\r') {
    // EOL found; terminate and try to parse
    _nmea_buf[_nmea_ptr] = '\0';
    _parse();
    _nmea_ptr = 0;
    return _gga_received && _rmc_received;
  }

  _nmea_buf[_nmea_ptr++] = c;
  if (_nmea_ptr == MAX_NMEA_LENGTH) { // unusual NMEA length; start over
    _nmea_ptr = 0;
  }

  return false;
}

/***********************************************
 * 
 */
void GPSClass::reset_buffer(bool flush_serial) {
  _gga_received = false;
  _rmc_received = false;
  _nmea_ptr = 0;
  _nmea_buf[0] = '\0';

  if (flush_serial) {
    //DEBUG_PRINTLN("Start flushing GPS serial.");
    while (_serial.available() > 0) {
      _serial.read();
    }
    //DEBUG_PRINTLN("Flush GPS serial done.");
  }
}

