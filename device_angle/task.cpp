#include "task.h"

Task::Task() {
  PT_INIT(&_pt);
  _wakeup = 0;
}

void Task::set_wakeup_time(uint32_t seconds) {
  _wakeup = seconds;
}

void Task::reset_wakeup_time() {
  _wakeup = 0;
}

uint32_t Task::get_wakeup_time() {
  return _wakeup;
}
