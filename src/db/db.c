#include "gui/gui.h"
#include "core/token.h"
#include <string.h>
#include <stdlib.h>


static int
compare_filename(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return strcmp(d.img.images[ia[0]].filename, d.img.images[ib[0]].filename);
}

static int
compare_rating(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  // higher rating comes first:
  return d.img.images[ib[0]].rating - d.img.images[ia[0]].rating;
}

static int
compare_labels(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return d.img.images[ia[0]].labels - d.img.images[ib[0]].labels;
}

static int
compare_createdate(const void *a, const void *b, void *arg)
{
  return 0;
}

void dt_db_update_collection()
{
  // filter
  d.img.collection_cnt = 0;
  for(int k=0; k<d.img.cnt; k++)
  {
    switch(d.img.collection_filter)
    {
    case s_prop_none:
      break;
    case s_prop_filename:
      if(!strstr(d.img.images[k].filename, d.img.collection_filter_text)) continue;
      break;
    case s_prop_rating:
      if(!(d.img.images[k].rating >= (int16_t)d.img.collection_filter_val)) continue;
      break;
    case s_prop_labels:
      if(!(d.img.images[k].labels & d.img.collection_filter_val)) continue;
      break;
    case s_prop_createdate:
      break;
    }
    d.img.collection[d.img.collection_cnt++] = k;
  }
  switch(d.img.collection_sort)
  {
  case s_prop_none:
    break;
  case s_prop_filename:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_filename, NULL);
    break;
  case s_prop_rating:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_rating, NULL);
    break;
  case s_prop_labels:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_labels, NULL);
    break;
  case s_prop_createdate:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_createdate, NULL);
    break;
  }
}

void dt_db_selection_add(uint32_t colid)
{
  if(colid >= d.img.collection_cnt) return;
  if(d.img.selection_cnt >= d.img.cnt) return;
  const uint32_t imgid = d.img.collection[colid];
  d.img.current_imgid = imgid;
  d.img.current_colid = colid;
  const uint32_t i = d.img.selection_cnt++;
  d.img.selection[i] = imgid;
  d.img.images[imgid].selected = 1;
}

void dt_db_selection_remove(uint32_t colid)
{
  if(d.img.current_colid == colid)
    d.img.current_imgid = d.img.current_colid = -1u;
  const uint32_t imgid = d.img.collection[colid];
  for(uint32_t i = 0; i < d.img.selection_cnt; i++)
  {
    if(d.img.selection[i] == imgid)
    {
      d.img.selection[i] = d.img.selection[--d.img.selection_cnt];
      d.img.images[imgid].selected = 0;
      return;
    }
  }
}

void dt_db_selection_clear()
{
  for(int i=0;i<d.img.selection_cnt;i++)
    d.img.images[d.img.selection[i]].selected = 0;
  d.img.selection_cnt = 0;
  d.img.current_imgid = d.img.current_colid = -1u;
}

const uint32_t *dt_db_selection_get()
{
  // TODO: sync sorting criterion with collection
  qsort_r(d.img.selection, d.img.selection_cnt, sizeof(d.img.selection[0]), compare_filename, NULL);
  return d.img.selection;
}
