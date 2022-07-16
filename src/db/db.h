#pragma once
#include <stdint.h>

typedef enum dt_image_label_t
{
  s_image_label_none   = 0,
  s_image_label_red    = 1<<0,
  s_image_label_green  = 1<<1,
  s_image_label_blue   = 1<<2,
  s_image_label_yellow = 1<<3,
  s_image_label_purple = 1<<4,
}
dt_image_label_t;

typedef struct ap_image_t
{
  int imgid;          // imgid
  char filename[252]; // filename
  char path[512];
  uint32_t hash;      // murmur3(filename)
  uint32_t thumbnail;
  int16_t rating;    // -1u reject 0 1 2 3 4 5 stars
  uint16_t labels;    // each bit is one colour label flag
  uint16_t selected;
  uint16_t res;
} ap_image_t;

// forward declare for stringpool.h so we don't have to include it here.
typedef struct dt_stringpool_entry_t dt_stringpool_entry_t;
typedef struct dt_stringpool_t
{
  uint32_t entry_max;
  dt_stringpool_entry_t *entry;

  uint32_t buf_max;
  uint32_t buf_cnt;
  char *buf;
}
dt_stringpool_t;

// after changing filter and sort criteria, update the collection array
void dt_db_update_collection();
// add image to the list of selected images, O(1).
void dt_db_selection_add(uint32_t colid);
// remove image from the list of selected images, O(N).
void dt_db_selection_remove(uint32_t colid);
void dt_db_selection_clear();
// return sorted list of selected images
const uint32_t *dt_db_selection_get();
