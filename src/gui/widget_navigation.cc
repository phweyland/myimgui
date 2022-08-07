#include "gui/widget_navigation.hh"
#include "gui/widget_filebrowser.hh"
#include "imgui.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
extern "C" {
#include "db/rc.h"
#include "db/database.h"
#include "gui/gui.h"
#include "core/log.h"
#include "core/fs.h"
}

void ap_navigation_init(ap_navigation_widget_t *w, const int type)
{
  memset(w, 0, sizeof(*w));
  w->type = type;
  snprintf(w->sep, sizeof(w->sep), "%s", type == 0 ? "/" : type == 1 ? "|" : "/");
  w->button = "Open";
  char home[256];
  snprintf(home, sizeof(home), "%s/", getenv("HOME"));
  const char *node = type == 0 ? dt_rc_get(&d.rc, "gui/last_folder", "")
                               : type == 1 ? dt_rc_get(&d.rc, "gui/last_tag", "")
                                   : dt_rc_get(&d.rc, "gui/last_import", home);
  if(node[0])
    snprintf(w->node, sizeof(w->node), "%s", node);
  w->count = -1u;
}

static void _update_node(ap_navigation_widget_t *w)
{
  if(w->subnodes)
    free(w->subnodes);
  w->subnodes = 0;
  w->count = -1u;
}

void ap_navigation_open(ap_navigation_widget_t *w)
{
  if(d.img.type != w->type || w->count == -1u)
    ap_gui_switch_collection(w->node, w->type);
  if(w->count == -1u)
  {
    clock_t beg = clock();
    if(w->type == 2)
      w->count = ap_fs_get_subdirs(w->node, &w->subnodes);
    else
      w->count = ap_db_get_subnodes(w->node, w->type, &w->subnodes);
    clock_t end = clock();
    dt_log(s_log_perf, "[db] ran get nodes in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);
    w->selected = 0;
    w->button = "Open";
  }

  if(w->type == 2)
  {
    if(ImGui::Button("Import"))
    {
      const char *folder = ap_db_get_folder(w->node);
      if(folder && d.img.cnt)
      {
        d.col_tab_req = 1<<0;  // go to Folders tab
        ap_navigation_widget_t *wf = d.browser[0];
        snprintf(wf->node, sizeof(wf->node), "%s", folder);
        wf->node[strlen(wf->node) - 1] = '\0';  // remove the leading '/''
        ap_db_add_images(d.img.selection, d.img.selection_cnt);
      }
    }
    ImGui::SameLine();
  }
  char current[256];
  snprintf(current, sizeof(current), "%s", w->node[0] ? w->node : w->type == 2 ? "/" : "root");
  ImGui::Text("%s", current);

  ImGui::BeginChild("nodelist", ImVec2(0, ImGui::GetFontSize() * 10.0f), 0);

  char name[260];
  int selected = 0;
  ap_nodes_t *node = w->subnodes;
  if(w->type != 2)
  {
    snprintf(name, sizeof(name), "%s", "..");
    if(ImGui::Selectable(name, selected, 0))
    {// go up one node
      int len = strnlen(w->node, sizeof(w->node));
      char *c = w->node + len;
      while(c >= w->node && *c != w->sep[0])
        *(c--) = 0;
      if(c >= w->node && *c == w->sep[0])
        *c = 0;
      _update_node(w);
    }
    // display list of nodes
    for(int i = 0; i < w->count; i++)
    {
      snprintf(name, sizeof(name), "%s", node->name);
      selected = node->name == w->selected;
      if(ImGui::Selectable(name, selected, 0))
      {
        w->selected = node->name; // mark as selected
        // change node by appending to the string..
        int len = strnlen(w->node, sizeof(w->node));
        if(len)
        {
          char *c = w->node;
          snprintf(c+len, sizeof(w->node)-len-1, "%s%s", w->sep, node->name);
        }
        else
          snprintf(w->node, sizeof(w->node), "%s", node->name);
        _update_node(w);
      }
      node++;
    }
  }
  else // physical folders
  {
    for(int i = 0; i < w->count; i++)
    {
      snprintf(name, sizeof(name), "%s", node->name);
      selected = node->name == w->selected;
      if(ImGui::Selectable(name, selected, 0))
      {
        w->selected = node->name; // mark as selected
        // change cwd by appending to the string..
        int len = strnlen(w->node, sizeof(w->node));
        char *c = w->node;
        if(!strcmp(node->name, ".."))
        { // go up one dir
          c += len;
          *(--c) = 0;
          while(c > w->node && *c != '/') *(c--) = 0;
        }
        else
        { // append dir name
          snprintf(c + len, sizeof(w->node) - len - 1, "%s/", node->name);
        }
        _update_node(w);
      }
      node++;
    }
  }
  ImGui::EndChild();
}
