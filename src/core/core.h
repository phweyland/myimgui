#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
// some random helpers

#define MIN(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })
#define MAX(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a > _b ? _a : _b; })
#define CLAMP(a,m,M) (MIN(MAX((a), (m)), (M)))

#define LENGTH(a) ((sizeof (a)) / (sizeof(*(a))))
