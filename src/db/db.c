#include "gui/gui.h"
#include "core/token.h"
#include <string.h>
#include <stdlib.h>

void ap_db_set_images_rating(const uint32_t *sel, const uint32_t cnt, const int16_t rating)
{
  for(uint32_t i = 0; i < cnt; i++)
    d.img.images[sel[i]].rating = rating;
  ap_db_write_rating(sel, cnt, rating);
}

void ap_db_toggle_images_labels(const uint32_t *sel, const uint32_t cnt, const uint16_t labels)
{
  for(uint32_t i = 0; i < cnt; i++)
    d.img.images[sel[i]].labels ^= labels;
  ap_db_toggle_labels(sel, cnt, labels);
}
