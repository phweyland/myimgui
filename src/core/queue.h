#pragma once
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct ap_fifo_t
{
  uint32_t nb;
  uint32_t in;
  uint32_t out;
  uint32_t elt_size;
  void *elts;
} ap_fifo_t;

inline static int ap_fifo_init(ap_fifo_t *f, const uint32_t elt_size, const uint32_t fifo_size)
{
  f->in = f->out = 0;
  f->elt_size = elt_size;
  f->nb = fifo_size + 1;
  f->elts = malloc(f->nb * f->elt_size);
  return f->elts !=  NULL ? 0 : 1;
}

inline static void ap_fifo_clean(ap_fifo_t *f)
{
  if(f->elts != NULL)
    free(f->elts);
}

inline static void ap_fifo_empty(ap_fifo_t *f)
{
  f->in = f->out;
}

inline static int ap_fifo_push(ap_fifo_t *f, void *elt)
{
  uint32_t cnt = f->in >= f->out ? f->in-f->out : f->in+f->nb-f->out;
  if(cnt >= f->nb - 1)
    return 1;
  if(cnt)
  {
    uint32_t j = f->out;
    for(uint32_t i = 0; i < cnt; i++)
    {
      // the first uint32_t of each elt is used as id
      void *p = (char *)f->elts + f->elt_size * j;
      if((uint32_t *)p == (uint32_t *)elt)
        return 0;  // already in queue
      if(++j == f->nb)
        j = 0;
    }
  }
  memcpy((char *)f->elts + f->in * f->elt_size, (char *)elt, f->elt_size);
  f->in++;
  if(f->in == f->nb)
    f->in = 0;
  return 0;
}

inline static int ap_fifo_pop(ap_fifo_t *f, void *elt)
{
  if(f->out != f->in)
  {
    memcpy((char *)elt, (char *)f->elts + f->out * f->elt_size, f->elt_size);
    f->out++;
    if(f->out == f->nb)
      f->out = 0;
    return 0;
  }
  return 1;
}
