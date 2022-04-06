/** 
 * @file
 * @brief Logging service on SD card
 *
 * Declarations for logging messages on an SD card.
 *
 * @author Chaiporn Jaikaeo <chaiporn.j@ku.ac.th>
 *
 */
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdint.h>
#include <stdarg.h>

#include <stdint.h>
#include <SPI.h>
#include <SD.h>

#include "raw-storage.h"
#include "printf.h"
#include "time-utils.h"
#include "debug.h"

#ifdef DEBUG_LOGGER
#define LOGGER_DEBUG_PRINT   DEBUG_PRINT
#define LOGGER_DEBUG_PRINTLN DEBUG_PRINTLN
#define LOGGER_DEBUG_PRINTF  DEBUG_PRINTF
#else
#define LOGGER_DEBUG_PRINT(x)
#define LOGGER_DEBUG_PRINTLN(x)
#define LOGGER_DEBUG_PRINTF(...)
#endif

#define LOGGER_MAGIC_STRING  "CATTRACK-LOGGER"
#define LOGGER_MAGIC_LENGTH  (sizeof(LOGGER_MAGIC_STRING)-1)

#define MAX_MSG_LEN  1024

#define LOGGER_INVALID_VALUE  0xFFFFFFFFFFFFFFFF

class Logger {
public: 
  /******************************************************************
   * Construct a logging storage
   *
   * The logger is NOT ready for use until setup() is called
   */
  Logger(Sd2Card& card):_raw(card) { _cursor = LOGGER_INVALID_VALUE; }

  /******************************************************************
   * Set up the pointer to store logged messages on SD card
   * If overwrite is true, existing logs will be overwritten;
   * otherwise, new logs will be appended.
   *
   * @return true if the setup is sucessful; false otherwise
   */
  bool setup(uint32_t start_block,uint32_t capacity,bool overwrite);

  /******************************************************************
   * Write a logged message, printf-style
   */
  void printf(const char *fmt, ... );

private:
  RawStorage _raw;       ///< interface to raw-SD 
  uint64_t _start_block; ///< first block on SD to store logging state
  uint64_t _cursor;      ///< current cursor position from the first block
  uint64_t _capacity;    ///< maximum number of bytes
  void _save_cursor();
  void _load_cursor();
  void _save_magic();
  uint64_t _sd_position() { return (_start_block+1)*512 + _cursor; }
};

#ifdef LOGGING
extern Logger logger;
#define LOG_TS(args...)  \
  do { \
    logger.printf("%s ", get_current_time_str()); \
    logger.printf(args); \
    DEBUG_PRINTF("%s ", get_current_time_str()); \
    DEBUG_PRINTF(args); \
  } while (0)
#define LOG(args...)  \
  do { \
    logger.printf(args); \
    DEBUG_PRINTF(args); \
  } while (0)
#else
#define LOG_TS(args...) \
  do { \
    DEBUG_PRINTF("%s ", get_current_time_str()); \
    DEBUG_PRINTF(args); \
  } while (0)
#define LOG(args...) \
  do { \
    DEBUG_PRINTF(args); \
  } while (0)
#endif

#endif

