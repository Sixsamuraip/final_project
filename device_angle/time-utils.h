#ifndef __TIME_UTILS_H__
#define __TIME_UTILS_H__

#include <TimeLib.h>
#include <stdint.h>

void init_time();
void set_time(uint16_t year,
              uint8_t  month,
              uint8_t  day,
              uint8_t  hour,
              uint8_t  minute,
              uint8_t  second);
uint32_t get_total_seconds();
char* get_current_time_str();

#define get_hours()      hour()
#define get_minutes()    minute()
#define get_seconds()    second()
#define get_year()       year()
#define get_month()      month()
#define get_day()        day()

#endif
