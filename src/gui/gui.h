#pragma once
#include "db/rc.h"
#include "db/database.h"
#include "db/thumbnails.h"
#include "core/qvk_util.h"

// forward declare
typedef struct GLFWwindow GLFWwindow;
typedef struct ap_db_t ap_db_t;

typedef struct ap_img_t
{
  char node[512];
  int type;
  int cnt;
  ap_image_t *images;
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
