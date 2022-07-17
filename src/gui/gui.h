#pragma once
#include "db/rc.h"
#include "db/database.h"
#include "db/thumbnails.h"
#include "core/qvk_util.h"

// forward declare
typedef struct GLFWwindow GLFWwindow;
typedef struct ap_db_t ap_db_t;

// view modes, lighttable, darkroom, ..
typedef enum dt_gui_view_t
{
  s_view_lighttable = 0,
  s_view_map = 2,
  s_view_cnt,
}
dt_gui_view_t;

typedef enum dt_db_property_t
{
  s_prop_none        = 0, // select all
  s_prop_filename    = 1, // strstr
  s_prop_rating      = 2, // >=
  s_prop_labels      = 3, // &
  s_prop_createdate  = 4,
}
dt_db_property_t;

__attribute__((unused))
static const char*
dt_db_property_text =
{
  "none\0"
  "filename\0"
  "rating\0"
  "label\0"
  "create date\0"
  "\0"
};

typedef struct ap_img_t
{
  char node[512];
  int type;
  int cnt;
  ap_image_t *images;

  // current sort and filter criteria for collection
  dt_db_property_t collection_sort;
  dt_db_property_t collection_filter;
  int collection_filter_val;
  char collection_filter_text[16];
  // current query
  uint32_t *collection;
  uint32_t collection_cnt;

  // selection
  uint32_t *selection;
  uint32_t selection_cnt;
  uint32_t current_imgid;
  uint32_t current_colid;
} ap_img_t;

typedef struct ap_vk_t
{
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceMemoryProperties mem_properties;
  VkSampler tex_sampler_nearest;
  VkSampler tex_sampler;
  VkQueue queue;
  VkDeviceMemory image_buffer_memory;
  VkBuffer image_buffer;
  int image_buffer_size;
} ap_vk_t;

typedef struct apdt_t
{
  GLFWwindow *window;
  ap_vk_t vk;     //vulkan

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
  int fullscreen;
  int swapchainrebuild;

  int view_mode;

  ap_img_t img;           // current collection
  dt_thumbnails_t  thumbs;

  dt_rc_t rc;            // config file
  ap_db_t *db;
  char basedir[1024];
} apdt_t;

extern apdt_t d;
extern char *ap_name;

int ap_gui_init();
void ap_gui_cleanup();
void ap_gui_window_resize();
void ap_gui_switch_collection(const char *node, const int type);
void ap_gui_get_buffer(VkCommandPool *command_pool, VkCommandBuffer *command_buffer, VkFence *fence);
void ap_gui_image_edit(const uint32_t imgid);

// after changing filter and sort criteria, update the collection array
void dt_gui_update_collection();
// add image to the list of selected images, O(1).
void dt_gui_selection_add(uint32_t colid);
// remove image from the list of selected images, O(N).
void dt_gui_selection_remove(uint32_t colid);
void dt_gui_selection_clear();
// return sorted list of selected images
const uint32_t *dt_gui_selection_get();
