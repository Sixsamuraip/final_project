#include "card-utils.h"
#include "config.h"

/**************************
 * Find configuration, data, and log partitions on an SD card.
 *
 * If found, start sector and the number of sectors of each partition are
 * returned via the specified pointers.
 */
void get_data_and_log_partitions(
    Sd2Card& card,
    uint8_t*  config_part_idx,
    uint32_t* data_start, 
    uint32_t* data_size, 
    uint32_t* log_start, 
    uint32_t* log_size)
{
  mbr_t mbr;

  *config_part_idx = *data_start = *data_size = *log_start = *log_size = 0;
  card.readBlock(0,(uint8_t*)&mbr);

  if (mbr.mbrSig0 != 0x55 && mbr.mbrSig1 != 0xAA) {
    return;
  }

  for (uint8_t i = 0; i < 4; i++) {
    if (*config_part_idx == 0 && mbr.part[i].type == CONFIG_PARTITION_ID) {
      if (config_part_idx != nullptr) *config_part_idx = i+1;
    }
    if (*data_start == 0 && mbr.part[i].type == STORAGE_PARTITION_ID) {
      if (data_start != nullptr) *data_start = mbr.part[i].firstSector;
      if (data_size != nullptr) *data_size = mbr.part[i].totalSectors;
    }
    if (*log_start == 0 && mbr.part[i].type == LOG_PARTITION_ID) {
      if (log_start != nullptr) *log_start = mbr.part[i].firstSector;
      if (log_size != nullptr) *log_size = mbr.part[i].totalSectors;
    }
  }
}

/*********************************************************
 * Open specified file located in the root folder of the specified volume
 */
bool open_file_in_root(SdFile& file, SdVolume& volume, const char* filename, uint8_t oflag) {
  SdFile root;
  if (!root.openRoot(volume)) {
    return false;
  }

  if (!file.open(&root, filename, oflag)) {
    root.close();
    return false;
  }

  root.close();
  return true;
}
