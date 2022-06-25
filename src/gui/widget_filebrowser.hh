#pragma once
#include <dirent.h>

// store some state
struct dt_filebrowser_widget_t
{
  char cwd[PATH_MAX+100];       // current working directory
  struct dirent **ent;  // cached directory entries
  int ent_cnt;          // number of cached directory entries
  const char *selected; // points to selected file name ent->d_name
};

void dt_filebrowser_init(dt_filebrowser_widget_t *w);

void dt_filebrowser_cleanup(dt_filebrowser_widget_t *w);

int dt_filebrowser_filter_dir(const struct dirent *d);

int dt_filebrowser_filter_file(const struct dirent *d);

void dt_filebrowser_open(dt_filebrowser_widget_t *w);

int dt_filebrowser_display(dt_filebrowser_widget_t *w, const char mode); // 'f' or 'd'
