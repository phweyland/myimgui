#include "core/log.h"
#include "core/fs.h"
#include "db/thumbnails.h"
#include "gui/gui.h"
#include "gui/render.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <time.h>

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
  {
    free(d.img.images);
    free(d.img.collection);
    free(d.img.selection);
  }
  snprintf(d.img.node, sizeof(d.img.node), "%s", node);
  d.img.type = type;
  clock_t beg = clock();
  d.img.cnt = ap_db_get_images(node, type, &d.img.images);
  clock_t end = clock();
  dt_log(s_log_perf, "[db] ran get images in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);

  d.img.collection = malloc(sizeof(uint32_t) * d.img.cnt);
  d.img.selection = malloc(sizeof(uint32_t) * d.img.cnt);
  d.img.current_imgid = d.img.current_colid = -1u;
  dt_gui_update_collection();

  dt_rc_set(&d.rc, type == 0 ? "gui/last_folder" : "gui/last_tag", d.img.node);
  dt_rc_set_int(&d.rc, "gui/last_collection_type", d.img.type);
}

void ap_gui_image_edit(const uint32_t imgid)
{
  if(imgid >= d.img.cnt)
    return;
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "%svkdt %s/%s",
           dt_rc_get(&d.rc, "vkdt_folder", ""), d.img.images[imgid].path, d.img.images[imgid].filename);
  popen(cmd, "r");
}

static int
compare_filename(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return strcmp(d.img.images[ia[0]].filename, d.img.images[ib[0]].filename);
}

static int
compare_rating(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  // higher rating comes first:
  return d.img.images[ib[0]].rating - d.img.images[ia[0]].rating;
}

static int
compare_labels(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return d.img.images[ia[0]].labels - d.img.images[ib[0]].labels;
}

static int
compare_createdate(const void *a, const void *b, void *arg)
{
  const uint32_t *ia = a, *ib = b;
  return strcmp(d.img.images[ia[0]].datetime, d.img.images[ib[0]].datetime);
}

void dt_gui_update_collection()
{
  // filter
  d.img.collection_cnt = 0;
  for(int k=0; k<d.img.cnt; k++)
  {
    switch(d.img.collection_filter)
    {
    case s_prop_none:
      break;
    case s_prop_filename:
      if(!strstr(d.img.images[k].filename, d.img.collection_filter_text)) continue;
      break;
    case s_prop_rating:
      if(!(d.img.images[k].rating >= (int16_t)d.img.collection_filter_val)) continue;
      break;
    case s_prop_labels:
      if(!(d.img.images[k].labels & d.img.collection_filter_val)) continue;
      break;
    case s_prop_createdate:
      if(!strstr(d.img.images[k].datetime, d.img.collection_filter_text)) continue;
      break;
    }
    d.img.collection[d.img.collection_cnt++] = k;
  }
  switch(d.img.collection_sort)
  {
  case s_prop_none:
    break;
  case s_prop_filename:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_filename, NULL);
    break;
  case s_prop_rating:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_rating, NULL);
    break;
  case s_prop_labels:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_labels, NULL);
    break;
  case s_prop_createdate:
    qsort_r(d.img.collection, d.img.collection_cnt, sizeof(d.img.collection[0]), compare_createdate, NULL);
    break;
  }
}

void dt_gui_selection_add(uint32_t colid)
{
  if(colid >= d.img.collection_cnt) return;
  if(d.img.selection_cnt >= d.img.cnt) return;
  const uint32_t imgid = d.img.collection[colid];
  if(d.img.current_imgid == -1)
    d.img.current_imgid = imgid;
  if(d.img.current_colid == -1)
    d.img.current_colid = colid;
  const uint32_t i = d.img.selection_cnt++;
  d.img.selection[i] = imgid;
  d.img.images[imgid].selected = 1;
}

void dt_gui_selection_remove(uint32_t colid)
{
  if(d.img.current_colid == colid)
    d.img.current_imgid = d.img.current_colid = -1u;
  const uint32_t imgid = d.img.collection[colid];
  for(uint32_t i = 0; i < d.img.selection_cnt; i++)
  {
    if(d.img.selection[i] == imgid)
    {
      d.img.selection[i] = d.img.selection[--d.img.selection_cnt];
      d.img.images[imgid].selected = 0;
      return;
    }
  }
}

void dt_gui_selection_clear()
{
  for(int i=0;i<d.img.selection_cnt;i++)
    d.img.images[d.img.selection[i]].selected = 0;
  d.img.selection_cnt = 0;
  d.img.current_imgid = d.img.current_colid = -1u;
}

void ap_gui_selection_all()
{
  d.img.selection_cnt = 0;
  uint32_t imgid = d.img.collection[0];
  if(d.img.current_imgid == -1)
    d.img.current_imgid = imgid;
  if(d.img.current_colid == -1)
    d.img.current_colid = 0;
  for(int i=0;i<d.img.collection_cnt;i++)
  {
    imgid = d.img.collection[i];
    const uint32_t i = d.img.selection_cnt++;
    d.img.selection[i] = imgid;
    d.img.images[imgid].selected = 1;
  }
}

const uint32_t *dt_gui_selection_get()
{
  // TODO: sync sorting criterion with collection
  qsort_r(d.img.selection, d.img.selection_cnt, sizeof(d.img.selection[0]), compare_filename, NULL);
  return d.img.selection;
}
