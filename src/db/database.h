#pragma once
#include <sqlite3.h>
#include <stdint.h>

typedef struct ap_image_t ap_image_t;

typedef struct ap_db_t
{
  sqlite3 *handle;
} ap_db_t;

typedef struct ap_nodes_t
{
  char name[252]; // node name
  int parent;     // is parent
} ap_nodes_t;

int ap_import_darktable(const char *dt_dir, char * text, int *abort);
void ap_db_init();

int ap_db_get_subnodes(const char *parent, const int type, ap_nodes_t **nodes);
int ap_db_get_images(const char *node, const int type, ap_image_t **images);

void ap_db_write_rating(const uint32_t *sel, const uint32_t cnt, const int16_t rating);
void ap_db_toggle_labels(const uint32_t *sel, const uint32_t cnt, const uint16_t labels);
