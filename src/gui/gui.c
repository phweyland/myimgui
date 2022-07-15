#include "core/log.h"
#include "core/fs.h"
#include "db/thumbnails.h"
#include "gui/gui.h"
#include "gui/render.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <time.h>

apdt_t d;
char *ap_name = "apdt";

void ap_gui_window_resize()
{
  const int pwd = d.panel_width_frac * d.win_width;
  d.center_x = d.border_frac * d.win_width;
  d.center_y = d.border_frac * d.win_width;
  d.scale = -1.0f;
  d.panel_wd = dt_rc_get_int(&d.rc, "gui/right_panel_width", pwd);
  d.center_wd = d.win_width * (1.0f - 2.0f * d.border_frac) - d.panel_wd;
  d.center_ht = d.win_height - 2*d.border_frac * d.win_width;
  d.panel_ht = d.win_height;
}

int ap_gui_init()
{
  if(!glfwInit())
  {
    const char* description;
    glfwGetError(&description);
    dt_log(s_log_gui|s_log_err, "glfwInit() failed: %s", description);
    return 1;
  }
  dt_log(s_log_gui, "glfwGetVersionString() : %s", glfwGetVersionString() );
  if(!glfwVulkanSupported())
  {
    const char* description;
    glfwGetError(&description);
    dt_log(s_log_gui|s_log_err, "glfwVulkanSupported(): no vulkan support found: %s", description);
    glfwTerminate(); // for correctness, if in future we don't just exit(1)
    return 1;
  }

  char configfile[512];
  snprintf(configfile, sizeof(configfile), "%s/.config/%s/config.rc", getenv("HOME"), ap_name);
  dt_rc_init(&d.rc);
  dt_rc_read(&d.rc, configfile);

  d.panel_width_frac = 0.2f;
  d.border_frac = 0.02f;
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  // start "full screen"
  d.win_width  = mode->width;  //1920;
  d.win_height = mode->height; //1080;

  fs_basedir(d.basedir, sizeof(d.basedir));
  ap_gui_init_imgui();

  const int type = dt_rc_get_int(&d.rc, "gui/last_collection_type", 0);
  const char *node = dt_rc_get(&d.rc, type == 0 ? "gui/last_folder" : "gui/last_tag", "");
  ap_gui_switch_collection(node, type);

  return 0;
}

void ap_gui_cleanup()
{
  char configfile[512];
  snprintf(configfile, sizeof(configfile), "%s/.config/%s/config.rc", getenv("HOME"), ap_name);
  dt_rc_write(&d.rc, configfile);
  dt_rc_cleanup(&d.rc);
  glfwDestroyWindow(d.window);
  glfwTerminate();
}

void ap_gui_switch_collection(const char *node, const int type)
{
  ap_reset_vkdt_thumbnail();
  if(d.img.images)
    free(d.img.images);
  snprintf(d.img.node, sizeof(d.img.node), "%s", node);
  d.img.type = type;
  clock_t beg = clock();
  d.img.cnt = ap_db_get_images(node, type, &d.img.images);
  clock_t end = clock();
  dt_log(s_log_perf, "[db] ran get images in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);
  dt_rc_set(&d.rc, type == 0 ? "gui/last_folder" : "gui/last_tag", d.img.node);
  dt_rc_set_int(&d.rc, "gui/last_collection_type", d.img.type);
}
