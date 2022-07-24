#include <GLFW/glfw3.h>
#include "gui/gui.h"
#include "gui/render.h"
#include "gui/view.h"
#include "gui/maps.h"
#include "core/core.h"
#include "core/log.h"
#include "core/threads.h"
#include "core/signal.h"
#include "db/database.h"
#include <sys/stat.h>
#include <stdlib.h>

apdt_t d = {0};
char *ap_name = "apdt";

// from a stackoverflow answer. get the monitor that currently covers most of
// the window area.
static GLFWmonitor *get_current_monitor(GLFWwindow *window)
{
  int bestoverlap = 0;
  GLFWmonitor *bestmonitor = NULL;

  int wx, wy, ww, wh;
  glfwGetWindowPos(window, &wx, &wy);
  glfwGetWindowSize(window, &ww, &wh);
  int nmonitors;
  GLFWmonitor **monitors = glfwGetMonitors(&nmonitors);

  for (int i = 0; i < nmonitors; i++)
  {
    const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
    int mx, my;
    glfwGetMonitorPos(monitors[i], &mx, &my);
    int mw = mode->width;
    int mh = mode->height;

    int overlap =
      MAX(0, MIN(wx + ww, mx + mw) - MAX(wx, mx)) *
      MAX(0, MIN(wy + wh, my + mh) - MAX(wy, my));

    if (bestoverlap < overlap)
    {
      bestoverlap = overlap;
      bestmonitor = monitors[i];
    }
  }
  return bestmonitor;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  dt_view_keyboard(window, key, scancode, action, mods);
//  dt_gui_imgui_keyboard(window, key, scancode, action, mods);
  if(key == GLFW_KEY_F11 && action == GLFW_PRESS)
  {
    GLFWmonitor* monitor = get_current_monitor(d.window);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if(d.fullscreen)
    {
      glfwSetWindowPos(d.window, mode->width/8, mode->height/8);
      glfwSetWindowSize(d.window, mode->width/4 * 3, mode->height/4 * 3);
      d.fullscreen = 0;
    }
    else
    {
      glfwSetWindowPos(d.window, 0, 0);
      glfwSetWindowSize(d.window, mode->width, mode->height);
      d.fullscreen = 1;
    }
    d.swapchainrebuild = 1;
  }
}

static void scroll_callback(GLFWwindow *window, double xoff, double yoff)
{
  dt_view_mouse_scrolled(window, xoff, yoff);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  dt_view_mouse_button(window, button, action, mods);
}

static void mouse_position_callback(GLFWwindow* window, double x, double y)
{
  dt_view_mouse_position(window, x, y);
}

int main(int argc, char *argv[])
{
  char basedir[1024];
  snprintf(basedir, sizeof(basedir), "%s/.config/%s", getenv("HOME"), ap_name);
  mkdir(basedir, 0755);
  dt_log_init(s_log_err|s_log_gui);
  dt_log_init_arg(argc, argv);
  threads_global_init();
  dt_set_signal_handlers();
  ap_db_init();
  if(ap_gui_init())
  {
    dt_log(s_log_gui|s_log_err, "failed to init gui/swapchain");
    return 1;
  }
  glfwSetKeyCallback(d.window, key_callback);
  glfwSetMouseButtonCallback(d.window, mouse_button_callback);
  glfwSetCursorPosCallback(d.window, mouse_position_callback);
  glfwSetScrollCallback(d.window, scroll_callback);

  dt_thumbnails_vkdt_init();
  ap_map_tiles_init();

  // Main loop
  while (!glfwWindowShouldClose(d.window))
  {
      // Poll and handle events (inputs, window resize, etc.)
      // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
      // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
      // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
      // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
      glfwPollEvents();

      ap_gui_render_frame_imgui();
  }

  // Cleanup
  ap_gui_cleanup_imgui();
  dt_thumbnails_cleanup(&d.thumbs);
  ap_map_tiles_cleanup();
  ap_gui_cleanup_vulkan();
  ap_gui_cleanup();
  threads_shutdown();
  threads_global_cleanup();

  return 0;
}
