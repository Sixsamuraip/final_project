/** 
 * @file
 * @brief Circular storage on SD card
 *
 * Definitions for creating and maintaining circular buffers on an SD card.
 *
 * @author Chaiporn Jaikaeo <chaiporn.j@ku.ac.th>
 *
 */
#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <stdint.h>
#include <SPI.h>
#include <SD.h>

#include "raw-storage.h"

#ifdef DEBUG_STORAGE
#define STORAGE_DEBUG_PRINT   DEBUG_PRINT
#define STORAGE_DEBUG_PRINTLN DEBUG_PRINTLN
#define STORAGE_DEBUG_PRINTF  DEBUG_PRINTF
#else
#define STORAGE_DEBUG_PRINT(x)
#define STORAGE_DEBUG_PRINTLN(x)
#define STORAGE_DEBUG_PRINTF(...)
#endif

#define MAGIC_STRING  "CATTRACK"
#define MAGIC_LENGTH  (sizeof(MAGIC_STRING)-1)

#define STORAGE_INVALID_BLOCK   0xFFFFFFFFFFFFFFFF

template <class T> class Storage {
public: 
  /******************************************************************
   * Construct a circular storage
   *
   * The circular storage is NOT ready for use until setup() is called
   */
  Storage(Sd2Card& card):_raw(card) { _start_block = STORAGE_INVALID_BLOCK; }

  /******************************************************************
   * Set up the storage blocks on SD card
   * If overwrite is true, the existing blocks will be overwritten
   *
   * @return true if the setup is sucessful; false otherwise
   */
  bool setup(uint64_t start_block,uint64_t capacity,bool overwrite);

  /******************************************************************
   * Return the maximum capacity (in #records) of the circular buffer
   */
  uint64_t capacity() const { return _capacity; }

  /******************************************************************
   * Check if the circular buffer is empty
   * @return true if the buffer is empty; false otherwise
   */
  bool empty() const { return _used == 0; }

  /******************************************************************
   * Check if the circular buffer is full
   * @return true if the buffer is full; false otherwise
   */
  bool full() const { return _used == _capacity; }

  /******************************************************************
   * Return the number of free records left in the buffer
   */
  uint64_t free() const { return _capacity - _used; }

  /******************************************************************
   * Return the number of records stored in the buffer
   */
  uint64_t count() const { return _used; }

  /******************************************************************
   * Push a new data item into the storage
   * @return true if successful (i.e., the storage is available); false otherwise
   */
  bool push(const T& data);

  /******************************************************************
   * Remove the earliest data item from the storage
   * @return true if successful (i.e., the storage is not empty); false otherwise
   */
  bool pop();

  /******************************************************************
   * Retrieve the specified data item without popping it out
   */
  bool peek(T* data,uint64_t index);

  /******************************************************************
   * Retrieve the earliest data item without popping it out
   */
  bool top(T* data) { return peek(data,0); }

  /******************************************************************
   * Remove all items from the storage
   */
  void clear();

private:
  RawStorage _raw;       ///< interface to raw-SD 
  uint64_t _start_block; ///< first block on SD to store data
  uint64_t _capacity;    ///< maximum number of records
  uint64_t _first;       ///< index of the first used slot
  uint64_t _used;        ///< number of used items

  // compute byte position for the specified slot index
  uint64_t _position(uint64_t index)
  { return (index+_start_block+1)*512; }

  // compute index of the first free slot
  uint64_t _first_free() { return (_first+_used)%_capacity; }

  void _save_state();
  void _load_state();
  void _save_magic();
};

template <class T>
void Storage<T>::_save_state()
{
  uint8_t state[sizeof(_first)+sizeof(_used)];

  if (_start_block == STORAGE_INVALID_BLOCK) return;

  STORAGE_DEBUG_PRINTF("STORAGE: Saving state: first=%s used=%s\r\n",
      ui64toa(_first), ui64toa(_used));
  memcpy(state,&_first,sizeof(_first));
  memcpy(state+sizeof(_first),&_used,sizeof(_used));
  _raw.write(_start_block*512,state,sizeof(state));
}

template <class T>
void Storage<T>::_load_state()
{
  uint8_t state[sizeof(_first)+sizeof(_used)];
  uint8_t magic[MAGIC_LENGTH];

  if (_start_block == STORAGE_INVALID_BLOCK) return;

  STORAGE_DEBUG_PRINTLN("STORAGE: Checking magic");
  _raw.read(_start_block*512+512-MAGIC_LENGTH,magic,MAGIC_LENGTH);

  if (strncmp((char*)magic,MAGIC_STRING,MAGIC_LENGTH) != 0) {
    STORAGE_DEBUG_PRINTLN("STORAGE: Magic not found; reset state");
    _first = _used = 0;
    _save_magic();
  }
  else {
    STORAGE_DEBUG_PRINTLN("STORAGE: Magic found; loading state");
    _raw.read(_start_block*512,state,sizeof(state));
    memcpy(&_first,state,sizeof(_first));
    memcpy(&_used,state+sizeof(_first),sizeof(_used));
    STORAGE_DEBUG_PRINTF("STORAGE: State loaded, first=%s used=%s\r\n",
        ui64toa(_first), ui64toa(_used));
  }

}

template <class T>
void Storage<T>::_save_magic()
{
  if (_start_block == STORAGE_INVALID_BLOCK) return;

  STORAGE_DEBUG_PRINTLN("STORAGE: Saving magic");
  _raw.write(_start_block*512+512-MAGIC_LENGTH,(const uint8_t*)MAGIC_STRING,MAGIC_LENGTH);
}

template <class T>
bool Storage<T>::setup(uint64_t start_block,uint64_t capacity,bool overwrite) 
{
  _start_block = start_block;
  _capacity = capacity-1;
  if (overwrite) 
  {
    _first = 0;
    _used = 0;
    _save_magic();
    _save_state();
  }
  else
  {
    // retrieve previous state from the card
    _load_state();
  }

  return true;
}

template <class T>
bool Storage<T>::push(const T& data)
{
  if (_start_block == STORAGE_INVALID_BLOCK) return false;

  if (full()) return false;
  _raw.write(_position(_first_free()),(uint8_t*)&data,sizeof(T));
  _used++;
  _save_state();
  return true;
}

template <class T>
bool Storage<T>::pop()
{
  if (_start_block == STORAGE_INVALID_BLOCK) return false;

  if (empty()) return false;
  _first = (_first+1)%_capacity;
  _used--;
  _save_state();
  return true;
}

template <class T>
bool Storage<T>::peek(T* data,uint64_t index)
{
  if (_start_block == STORAGE_INVALID_BLOCK) return false;

  if (index >= _used) return false;
  _raw.read(_position((_first+index)%_capacity),(uint8_t*)data,sizeof(T));
  return true;
}

template <class T>
void Storage<T>::clear()
{
  if (_start_block == STORAGE_INVALID_BLOCK) return;

  _used = 0;
  _first = 0;
  _save_state();
}
#endif
