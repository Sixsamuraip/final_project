#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <cstddef>
#include <time.h>
#include "pt/pt.h"

#define TASK          PT_THREAD
#define TASK_BEGIN()  PT_BEGIN(&_pt)
#define TASK_END()    PT_END(&_pt)
#define TASK_YIELD()  PT_YIELD(&_pt)
#define TASK_WAIT_UNTIL(cond)  PT_WAIT_UNTIL(&_pt, cond)

#define TASK_TIMER_START(ts)            ts = millis();
#define TASK_TIMER_EXPIRED(ts,timeout)  ((millis()-ts) > (timeout))
#define TASK_DELAY(ms,tsVar) \
        do { \
          tsVar = millis(); \
          PT_WAIT_UNTIL(&_pt, millis()-tsVar >= (ms)); \
        } while (0);

#define TASK_SHORT_BLINK(ts,on) \
        do { \
          digitalWrite(LED_BUILTIN,HIGH); \
          TASK_DELAY(on,ts); \
          digitalWrite(LED_BUILTIN,LOW); \
        } while (0);


class Task {
protected:
  struct pt _pt;
  uint32_t _wakeup;

public:
  Task();
  void set_wakeup_time(uint32_t seconds);
  void reset_wakeup_time();
  uint32_t get_wakeup_time();
  virtual const char* get_name() = 0;
  virtual TASK(run()) = 0;
};

#endif
