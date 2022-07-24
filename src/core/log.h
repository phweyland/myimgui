#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// layered logging facility

typedef enum dt_log_mask_t
{
  s_log_none = 0,
  s_log_gui  = 1<<0,
  s_log_cli  = 1<<1,
  s_log_perf = 1<<2,
  s_log_mem  = 1<<3,
  s_log_db   = 1<<4,
  s_log_vk   = 1<<5,
  s_log_map  = 1<<6,
  s_log_err  = 1<<7,
  s_log_all  = -1ul,
}
dt_log_mask_t;

typedef struct dt_log_t
{
  dt_log_mask_t mask;
  double start_time;
}
dt_log_t;

extern dt_log_t dt_log_global;

int dt_log_init_arg(int argc, char *argv[]);
void dt_log_init(dt_log_mask_t verbose);
void dt_log(dt_log_mask_t mask, const char *format, ...);
