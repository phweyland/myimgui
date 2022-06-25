#include <GLFW/glfw3.h>
#include "gui/gui.h"
#include "gui/render.h"
#include "core/log.h"
#include "core/threads.h"
#include "core/signal.h"
#include "db/database.h"
#include <sys/stat.h>
#include <stdlib.h>

int main(int, char**)
{
  char basedir[1024];
  snprintf(basedir, sizeof(basedir), "%s/.config/%s", getenv("HOME"), ap_name);
  mkdir(basedir, 0755);
  dt_log_init(s_log_err|s_log_gui);
  threads_global_init();
  dt_set_signal_handlers();
  if(ap_gui_init())
  {
    dt_log(s_log_gui|s_log_err, "failed to init gui/swapchain");
    return 1;
  }
  ap_db_init();

  // Main loop
  while (!glfwWindowShouldClose(apdt.window))
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
  ap_gui_cleanup();
  threads_shutdown();
  threads_global_cleanup();

  return 0;
}
