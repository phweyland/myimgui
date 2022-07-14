#include "core/log.h"
#include "core/core.h"

dt_log_t dt_log_global;

// this can be done to parse "-d level" from
// the command line to add more verbose output
// returns the index to the last argument that has been used here.
int dt_log_init_arg(int argc, char *argv[])
{
  const char *id[] = {
    "none",
    "gui",
    "cli",
    "perf",
    "mem",
    "db",
    "vk",
    "err",
    "all",
  };
  int num = sizeof(id)/sizeof(id[0]);
  uint64_t verbose = s_log_err; // error only by default
  int lastarg = 0; // will return the last index we used
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-d") && i < argc-1)
    {
      lastarg = ++i;
      for(int j=0;j<num;j++)
      {
        if(!strcmp(argv[i], id[j]))
        {
          if(j == 0)          verbose = 0ul;
          else if(j == num-1) verbose = -1ul;
          else                verbose |= 1<<(j-1);
        }
      }
    }
  }
  // user parameters add to the mask:
  dt_log_global.mask |= verbose;
  if(verbose == 0ul) dt_log_global.mask = verbose;
  return lastarg;
}

// call this first once
void dt_log_init(dt_log_mask_t verbose)
{
  dt_log_global.mask = verbose;
  dt_log_global.start_time = dt_time();
}

void dt_log(
    dt_log_mask_t mask,
    const char *format,
    ...)
{
  const char *pre[] = {
    "",
    "[gui]",
    "[cli]",
    "[perf]",
    "[mem]",
    "[db]",
    "[vk]",
    "\e[31m[ERR]\e[0m",
  };

  if(dt_log_global.mask & mask)
  {
    char str[2048];
    int index = mask ? 32-__builtin_clz(mask) : 0;
    if(index > sizeof(pre)/sizeof(pre[0])) index = 0;
    double end = dt_time();
    snprintf(str, sizeof(str), "%.6f %s %s\n", (float)(end - dt_log_global.start_time), pre[index], format);
    va_list args;
    va_start(args, format);
    vfprintf(stdout, str, args);
    va_end(args);
  }
}
