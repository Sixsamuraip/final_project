/** 
 * @file
 * Utility class definition for raw access to SD card
 *
 * @author Chaiporn Jaikaeo <chaiporn.j@ku.ac.th>
 *
 */

//#define DEBUG
//#define DEBUG_RAW
#include "raw-storage.h"

/**********************************************
 *
 */
void RawStorage::_cache_read(uint64_t pos) {
  if (pos != _cached_pos) {
    RAW_DEBUG_PRINTF("RAW: reading block #%s into cache\r\n", ui64toa(pos));
    _card.readBlock(pos,_cached_block);
    _cached_pos = pos;
  }
  else {
    RAW_DEBUG_PRINTF("RAW: block #%d already cached\r\n", ui64toa(pos));
  }
}

/**********************************************
 *
 */
void RawStorage::_cache_write(uint64_t pos) {
  RAW_DEBUG_PRINTF("RAW: writing cache into block #%d\r\n", ui64toa(pos));
  _card.writeBlock(pos,_cached_block);
  _cached_pos = pos;
}

/**********************************************
 *
 */
bool RawStorage::read(uint64_t offset, uint8_t* data, uint16_t len) {
  uint64_t block_pos;
  uint16_t block_offset;

  block_pos = BLOCK_POSITION(offset);
  block_offset = BLOCK_OFFSET(offset);

  if (block_offset != 0) {
    // data to be read located at the end of the block;
    _cache_read(block_pos);
    if (len + block_offset < 512) {
      RAW_DEBUG_PRINTF("RAW: copying cache[%d] -> mem[%d], count=%d\r\n",
          block_offset,
          data,
          len);
      memcpy(data,_cached_block+block_offset,len);
      len = 0;
    }
    else {
      // the read data continues into the next block;
      // read to the end of the block and adjust data/len
      RAW_DEBUG_PRINTF("RAW: copying cache[%d] -> mem[%d], count=%d\r\n",
          block_offset,
          data,
          512-block_offset);
      memcpy(data,_cached_block+block_offset,512-block_offset);
      len -= 512-block_offset;
      data += 512-block_offset;
      block_pos++;
    }
  }

  // keep reading whole blocks while possible
  while (len >= 512) {
    _cache_read(block_pos);
    RAW_DEBUG_PRINTF("RAW: copying cache[%d] -> mem[%d], count=%d\r\n",
        0,
        data,
        512);
    memcpy(data,_cached_block,512);
    data += 512;
    len -= 512;
    block_pos++;
  }

  if (len > 0) { // take care of possible remaining bytes
    _cache_read(block_pos);
    RAW_DEBUG_PRINTF("RAW: copying cache[%d] -> mem[%d], count=%d\r\n",
        0,
        data,
        len);
    memcpy(data,_cached_block,len);
  }

  return true;
}

/**********************************************
 *
 */
bool RawStorage::write(uint64_t offset, const uint8_t* data, uint16_t len) {
  uint64_t block_pos;
  uint16_t block_offset;

  block_pos = BLOCK_POSITION(offset);
  block_offset = BLOCK_OFFSET(offset);

  if (block_offset != 0) {
    // data to be written located at the end of the block;
    // need to read the target block out first
    _cache_read(block_pos);
    if (len + block_offset < 512) {
      RAW_DEBUG_PRINTF("RAW: copying mem[%d] -> cache[%d], count=%d\r\n",
          data,
          block_offset,
          len);
      memcpy(_cached_block+block_offset,data,len);
      len = 0;
    }
    else {
      // the written data continues into the next block;
      // write to the end of the block and adjust data/len
      RAW_DEBUG_PRINTF("RAW: copying mem[%d] -> cache[%d], count=%d\r\n",
          data,
          block_offset,
          512-block_offset);
      memcpy(_cached_block+block_offset,data,512-block_offset);
      len -= 512-block_offset;
      data += 512-block_offset;
    }
    _cache_write(block_pos);
    block_pos++;
  }

  // keep writing whole blocks while possible
  while (len >= 512) {
    RAW_DEBUG_PRINTF("RAW: copying mem[%d] -> cache[%d], count=%d\r\n",
        data,
        0,
        512);
    memcpy(_cached_block,data,512);
    _cache_write(block_pos);
    data += 512;
    len -= 512;
    block_pos++;
  }

  if (len > 0) { // take care of possible remaining bytes
    _cache_read(block_pos);
    RAW_DEBUG_PRINTF("RAW: copying mem[%d] -> cache[%d], count=%d\r\n",
        data,
        0,
        len);
    memcpy(_cached_block,data,len);
    _cache_write(block_pos);
  }

  return true;
}

