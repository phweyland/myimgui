#pragma once
#include "../db/rc.h"
#include "../db/database.h"

// forward declare
typedef struct GLFWwindow GLFWwindow;
typedef struct ap_db_t ap_db_t;

typedef struct ap_image_t
{
  int imgid;          // imgid = murmur3(filename)
  char filename[252]; // filename
  char path[512];
} ap_image_t;

typedef struct apdt_t
{
  GLFWwindow *window;
  int theme;

  int win_width;
  int win_height;

  float panel_width_frac;   // width of the side panel as fraction of the total window width
  float border_frac;        // width of border between image and panel

  // center window configuration
  float scale;
  int center_x, center_y;
  int center_wd, center_ht;
  int panel_wd;
  int panel_ht;

  int image_cnt;
  ap_image_t *images;

  dt_rc_t rc;            // config file
  ap_db_t *db;
} apdt_t;

extern apdt_t apdt;
extern const char *ap_name;

int ap_gui_init();
void ap_gui_cleanup();
void ap_gui_window_resize();
void ap_gui_switch_collection(const char *node, const int type);
