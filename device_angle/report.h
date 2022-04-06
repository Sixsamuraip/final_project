#ifndef __REPORT_H__
#define __REPORT_H__

#include <stdint.h>

struct ReportItem {
  uint8_t year,month,day;
  uint8_t hour,minute,second;
  uint16_t vbat;  // in mV
  int32_t latitude,longitude; // unit of 1/100000 degrees
  uint8_t quality,satellites;
  uint16_t temperature;
  uint32_t last_heard_from_gw;
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t glo_ag_x;
  int16_t glo_ag_y;
  int16_t glo_ag_z;
} __attribute__((packed));

#endif
