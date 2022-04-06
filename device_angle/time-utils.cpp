#include <TimeLib.h>
#include <stdio.h>
#include "time-utils.h"

char _datetime_string[30];

/************************************
 *
 */
void init_time() {
  set_time(2000,1,1,0,0,0);
}

/************************************
 *
 */
void set_time(uint16_t year,
              uint8_t  month,
              uint8_t  day,
              uint8_t  hour,
              uint8_t  minute,
              uint8_t  second) {
  setTime(hour,minute,second,day,month,year);
}

/************************************
 *
 */
uint32_t get_total_seconds() {
  return now();
}

/************************************
 *
 */
char* get_current_time_str() {
  sprintf(_datetime_string, "%d-%02d-%02d %02d:%02d:%02d",
    get_year(),
    get_month(),
    get_day(),
    get_hours(),
    get_minutes(),
    get_seconds());
  return _datetime_string;
}
