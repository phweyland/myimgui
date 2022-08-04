#pragma once
#include "gui/gui.h"
#include "gui/view.h"
#include <GLFW/glfw3.h>

#define TILE_SERVER   "https://a.tile.openstreetmap.org/%s.png" // the tile map server url
#define TILE_SIZE     256                                 // the expected size of tiles in pixels, e.g. 256x256px
#define MAX_ZOOM      19                                  // the maximum zoom level provided by the server
#define MAX_THREADS   2                                   // the maximum threads to use for downloading tiles (OSC strictly forbids more than 2)
#define USER_AGENT    "myap"                              // change this to represent your own app if you extend this code

typedef struct ap_map_bounds_t
{
  double xm; double ym;
  double xM; double yM;
} ap_map_bounds_t;

typedef struct ap_tile_t
{
  int z; // zoom    [0......20]
  int x; // x index [0......z] z+1 values
  int y; // y index [0......z] z+1 values (z+1)^2 combinations
  uint64_t zxy[3];
  struct ap_tile_t *prev;    // dlist for lru cache
  struct ap_tile_t *next;
  uint32_t thumbnail;
  int thumb_status;
} ap_tile_t;

typedef struct ap_map_t
{
  // window coordinates on map (0.0, 1.0)
  double wd; double ht;
  double xm; double xM;
  double ym; double yM;
  double prevx; double prevy;
  double pixel_size;

  int z;
  int drag;
  int source;

  //mouse
  double mwx; double mwy;
  double mmx; double mmy;
  uint32_t mtile;

  // tiles vulkan images queue
  dt_thumbnails_t thumbs;

  // region
  int region_max;
  int region_cnt;
  int region_cov;
  uint32_t *region;

  // tiles collection
  int tiles_max;
  ap_tile_t *tiles;
// threads_mutex_t       lru_lock; // currently not needed, only using lru cache in gui thread
  ap_tile_t *lru;   // least recently used tile, delete this first
  ap_tile_t *mru;   // most  recently used tile, append here
} ap_map_t;

static inline void map_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_L)
      dt_view_switch(s_view_lighttable);
    else if(key == GLFW_KEY_A)
      d.debug = d.debug ? 0 : 1;
  }
}

void map_mouse_scrolled(GLFWwindow* window, double xoff, double yoff);
void map_mouse_button(GLFWwindow* window, int button, int action, int mods);
void map_mouse_position(GLFWwindow* window, double x, double y);

void ap_map_get_region();

void ap_map_tiles_init();
void ap_map_cleanup();

double ap_map2win_x(const double mx);
double ap_map2win_y(const double my);
