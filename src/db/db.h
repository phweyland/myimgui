#pragma once
#include <stdint.h>

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
