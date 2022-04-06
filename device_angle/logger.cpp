/** 
 * @file
 * @brief Logging service on SD card
 *
 * Definitions for logging messages on an SD card.
 *
 * @author Chaiporn Jaikaeo <chaiporn.j@ku.ac.th>
 *
 */

//#define DEBUG
//#define DEBUG_LOGGER
#include "logger.h"

/*************************************
 *
 */
void Logger::_save_magic()
{
  if (_start_block == LOGGER_INVALID_VALUE) return;

  LOGGER_DEBUG_PRINTLN("LOGGER: Saving magic");
  _raw.write(_start_block*512+512-LOGGER_MAGIC_LENGTH,(const uint8_t*)LOGGER_MAGIC_STRING,LOGGER_MAGIC_LENGTH);
}

/*************************************
 *
 */
void Logger::_save_cursor()
{
  if (_start_block == LOGGER_INVALID_VALUE) return;

  LOGGER_DEBUG_PRINTLN("LOGGER: Saving cursor");
  _raw.write(_start_block*512,(uint8_t*)&_cursor,sizeof(_cursor));
}

/*************************************
 *
 */
void Logger::_load_cursor() {
  uint8_t magic[LOGGER_MAGIC_LENGTH];

  if (_start_block == LOGGER_INVALID_VALUE) return;

  LOGGER_DEBUG_PRINTLN("LOGGER: Checking magic");
  uint32_t x1, x2;
  _raw.read(_start_block*512+512-LOGGER_MAGIC_LENGTH,magic,LOGGER_MAGIC_LENGTH);

  if (strncmp((char*)magic,LOGGER_MAGIC_STRING,LOGGER_MAGIC_LENGTH) != 0) {
    LOGGER_DEBUG_PRINTLN("LOGGER: Magic not found; reset cursor");
    _cursor = 0;
    _save_magic();
  }
  else {
    LOGGER_DEBUG_PRINTLN("LOGGER: Magic found; loading cursor");
    _raw.read(_start_block*512,(uint8_t*)&_cursor,sizeof(_cursor));
    LOGGER_DEBUG_PRINTF("LOGGER: Cursor loaded, position=%s\r\n", ui64toa(_cursor));
  }
}

/*************************************
 *
 */
bool Logger::setup(uint32_t start_block,uint32_t capacity,bool overwrite)
{
  _start_block = start_block;
  _capacity = (capacity-1)*512;
  if (overwrite) 
  {
    _cursor = 0;
    _save_magic();
    _save_cursor();
  }
  else
  {
    // retrieve previous state from the card
    _load_cursor();
  }

  return true;
}

/*************************************
 *
 */
void Logger::printf(const char *fmt, ... )
{
  if (_start_block == LOGGER_INVALID_VALUE) return;

  char buf[MAX_MSG_LEN];
  va_list args;
  va_start (args, fmt );
  vsnprintf(buf, MAX_MSG_LEN, fmt, args);
  va_end(args);
  uint16_t len = strlen(buf);
  if (_cursor + len < _capacity) {
    _raw.write(_sd_position(),(uint8_t*)buf,len);
    LOGGER_DEBUG_PRINTF("LOGGER: write %d bytes at cursor=%s (sd-pos=%s)\r\n",
        len,
        ui64toa(_cursor),
        ui64toa(_sd_position()));
    _cursor += len;
    _save_cursor();
  }
}

