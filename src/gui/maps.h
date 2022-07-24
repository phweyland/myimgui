#pragma once
#include "gui/gui.h"
#include "gui/view.h"
#include <GLFW/glfw3.h>

#define TILE_SERVER   "https://a.tile.openstreetmap.org/" // the tile map server url
#define TILE_SIZE     256                                 // the expected size of tiles in pixels, e.g. 256x256px
#define MAX_ZOOM      19                                  // the maximum zoom level provided by the server
#define MAX_THREADS   2                                   // the maximum threads to use for downloading tiles (OSC strictly forbids more than 2)
#define USER_AGENT    "myap"                              // change this to represent your own app if you extend this code

typedef struct ap_map_t
{
//  double cx; double cy; // window center on map (0.0, 1.0)
  double wd; double ht;
  double xm; double xM;  // window coordinates on map
  double ym; double yM;
  double prevx; double prevy;
  double pixel_size;
  int z;
  int drag;

  // tiles queue
  ap_fifo_t tiles;
  int tiles_abort;
} ap_map_t;

typedef struct ap_map_box_t
{
  double b00; //lon1;
  double b01; //lat1;
  double b10; //lon2;
  double b11; //lat2;
} dt_map_box_t;

typedef struct ap_tile_t
{
  int z; // zoom    [0......20]
  int x; // x index [0......z] z+1 values
  int y; // y index [0......z] z+1 values (z+1)^2 combinations
  char subdir[150];
  char dir[150];
  char file[50];
  char path[200];
  char url[200];
  char label[50];
  struct ap_map_box_t b;
} ap_tile_t;

typedef enum ap_tile_state_t
{
  s_tile_unavailable = 0, // tile not available
  s_tile_loaded,          // tile has been loaded into  memory
  s_tile_downloading,     // tile is downloading from server
  s_tile_ondisk,          // tile is saved to disk, but not loaded into memory
  s_tile_cnt,
} ap_tile_state_t;

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

void dt_map_tiles_init();
void ap_map_tiles_cleanup();
