#pragma once
#include <sqlite3.h>

typedef struct ap_db_t
{
  sqlite3 *handle;
} ap_db_t;

int ap_import_darktable(const char *dt_dir, char * text, int *abort);
void ap_init_db();
