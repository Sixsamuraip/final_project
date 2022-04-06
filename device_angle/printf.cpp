#include <stdarg.h>
#include <Stream.h>

#define MAX_RESULT_LENGTH 512

// Adapted from https://playground.arduino.cc/Main/Printf

void printf(Stream& stream, const char *fmt, ... )
{
  char buf[MAX_RESULT_LENGTH];
  va_list args;
  va_start (args, fmt );
  vsnprintf(buf, MAX_RESULT_LENGTH, fmt, args);
  va_end (args);
  stream.print(buf);
}

void printf(Stream& stream, const __FlashStringHelper *fmt, ... )
{
  char buf[MAX_RESULT_LENGTH];
  va_list args;
  va_start (args, fmt);
#ifdef __AVR__
  vsnprintf_P(buf, sizeof(buf), (const char *)fmt, args); // progmem for AVR
#else
  vsnprintf(buf, sizeof(buf), (const char *)fmt, args); // for the rest of the world
#endif
  va_end(args);
  stream.print(buf);
}

/*****************************************
 * Convert 64-bit unsigned integer to decimal
 */
char* ui64toa(uint64_t v) {
  static char buf[30];
  uint8_t pos = sizeof(buf)-1;
  buf[pos--] = '\0';

  if (v == 0) {
    buf[pos--] = '0';
  }
  else {
    while (v) {
      buf[pos--] = '0'+(v%10);
      v /= 10;
    }
  }
  return buf+pos+1;
}
