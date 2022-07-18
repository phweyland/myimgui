
#include "imgui.h"
#include <GLFW/glfw3.h>

#include "gui/themes.hh"
#include "gui/widget_filebrowser.hh"
#include "gui/widget_navigation.hh"
#include "gui/widget_thumbnail.hh"

extern "C" {
#include "core/core.h"
#include "gui/gui.h"
#include "gui/view.h"
#include "db/database.h"
}

char impdt_text[256];
int impdt_taskid;
dt_filebrowser_widget_t vkdtbrowser;
dt_filebrowser_widget_t darktablebrowser;
ap_navigation_widget_t folderbrowser;
ap_navigation_widget_t tagbrowser;

void render_lighttable()
{
  const float spacing = 5.0f;
  // right panel
  {
    ImGui::SetNextWindowPos(ImVec2(d.win_width - d.panel_wd, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.panel_wd, d.panel_ht), ImGuiCond_Always);
    ImGui::Begin("RightPanel", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    if(ImGui::CollapsingHeader("Collection"))
    {
      ImGui::Indent();
      int32_t filter_prop = d.img.collection_filter;
      int32_t sort_prop = d.img.collection_sort;
      int32_t comp_prop = d.img.collection_comp;

      if(ImGui::Combo("sort", &sort_prop, dt_db_property_text))
      {
        d.img.collection_sort = static_cast<dt_db_property_t>(sort_prop);
        dt_gui_update_collection();
      }
      if(ImGui::Combo("filter", &filter_prop, dt_db_property_text))
      {
        d.img.collection_filter = static_cast<dt_db_property_t>(filter_prop);
        dt_gui_update_collection();
      }
      if(filter_prop == s_prop_rating || filter_prop == s_prop_labels)
      {
        const float item_width = ImGui::CalcItemWidth();
        ImGui::PushItemWidth(ImGui::GetFontSize() * 3);
        if(ImGui::Combo("##comp", &comp_prop, ap_db_comp_text))
        {
          d.img.collection_comp = static_cast<ap_db_comp_t>(comp_prop);
          dt_gui_update_collection();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0.0f, spacing);
        ImGui::PushItemWidth(item_width - ImGui::GetFontSize() * 3 - spacing);
        int filter_val = static_cast<int>(d.img.collection_filter_val);
        if(ImGui::InputInt("filter value", &filter_val, 1, 1, 0))
        {
          if(filter_val >= -1 && filter_val <= 5)
            d.img.collection_filter_val = static_cast<uint64_t>(filter_val);
          dt_gui_update_collection();
        }
        ImGui::PopItemWidth();
      }
      else if(filter_prop == s_prop_filename || filter_prop == s_prop_createdate)
      {
        if(ImGui::InputText("filter text", d.img.collection_filter_text, IM_ARRAYSIZE(d.img.collection_filter_text)))
        {
          dt_gui_update_collection();
        }
      }
      ImGui::Unindent();

      ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
      if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
      {
        if (ImGui::BeginTabItem("Folders"))
        {
          ap_navigation_open(&folderbrowser);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tags"))
        {
          ap_navigation_open(&tagbrowser);
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
      ImGui::Separator();
    }

    if(ImGui::CollapsingHeader("Images info"))
    {
      ImGui::Text("Filename:");
      ImGui::SameLine(0.0f, spacing);
      ImGui::Text("%s", (d.img.selection_cnt != 1) ?  "-" : d.img.images[d.img.selection[0]].filename);

      ImGui::Text("Date Time:");
      ImGui::SameLine(0.0f, spacing);
      ImGui::Text("%s", (d.img.selection_cnt != 1) ?  "-" : d.img.images[d.img.selection[0]].datetime);

      ImGui::Text("Longitude:");
      ImGui::SameLine(0.0f, spacing);
      if(d.img.selection_cnt != 1 || isnan(d.img.images[d.img.selection[0]].longitude))
        ImGui::Text("-");
      else
        ImGui::Text("%c %010.6f", (d.img.images[d.img.selection[0]].longitude < 0) ? 'W' : 'E', fabs(d.img.images[d.img.selection[0]].longitude));

      ImGui::Text("Latitude:");
      ImGui::SameLine(0.0f, spacing);
      if(d.img.selection_cnt != 1 || isnan(d.img.images[d.img.selection[0]].latitude))
        ImGui::Text("-");
      else
        ImGui::Text("%c %09.6f", (d.img.images[d.img.selection[0]].latitude < 0) ? 'S' : 'N', fabs(d.img.images[d.img.selection[0]].latitude));

      ImGui::Text("Altitude:");
      ImGui::SameLine(0.0f, spacing);
      if(d.img.selection_cnt != 1 || isnan(d.img.images[d.img.selection[0]].altitude))
        ImGui::Text("-");
      else
        ImGui::Text("%0.2f m", d.img.images[d.img.selection[0]].altitude);
    }

    if(ImGui::CollapsingHeader("Preferences"))
    {
      const char* combo_preview_value = themes_name[d.theme];  // Pass in the preview value visible before opening the combo (it could be anything)
      if (ImGui::BeginCombo("theme", combo_preview_value, 0))
      {
        for (int n = 0; themes_name[n]; n++)
        {
          const bool is_selected = (d.theme == n);
          if (ImGui::Selectable(themes_name[n], is_selected))
          {
            d.theme = n;
            themes_set[n]();
            dt_rc_set_int(&d.rc, "gui/theme", n);
          }
          // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
          if (is_selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }

      if(ImGui::Button("Vkdt folder"))
      {
        const char* vk_dir = dt_rc_get(&d.rc, "vkdt_folder", "");
        if(vk_dir[0])
          snprintf(vkdtbrowser.cwd, sizeof(vkdtbrowser.cwd), "%s", vk_dir);
        dt_filebrowser_open(&vkdtbrowser);
      }
      if(dt_filebrowser_display(&vkdtbrowser, 'd'))
      { // "ok" pressed
        dt_rc_set(&d.rc, "vkdt_folder", vkdtbrowser.cwd);
        dt_filebrowser_cleanup(&vkdtbrowser); // reset all but cwd
      }
      ImGui::SameLine();
      ImGui::Text("%s", dt_rc_get(&d.rc, "vkdt_folder", ""));

      int ipl = d.ipl;
      if(ImGui::InputInt("number of images per line", &ipl, 1, 3, 0))
      {
        if(ipl >= 1 && ipl <= 20)
          d.ipl = ipl;
        dt_rc_set_int(&d.rc, "gui/images_per_line", d.ipl);
      }

      if(ImGui::CollapsingHeader("Import darktable"))
      {
        if(ImGui::Button("Darktable folder"))
        {
          const char* dt_dir = dt_rc_get(&d.rc, "darktable_folder", "");
          if(dt_dir[0])
            snprintf(darktablebrowser.cwd, sizeof(darktablebrowser.cwd), "%s", dt_dir);
          dt_filebrowser_open(&darktablebrowser);
        }
        if(dt_filebrowser_display(&darktablebrowser, 'd'))
        { // "ok" pressed
          dt_rc_set(&d.rc, "darktable_folder", darktablebrowser.cwd);
          dt_filebrowser_cleanup(&darktablebrowser); // reset all but cwd
        }
        ImGui::SameLine();
        ImGui::Text("%s", dt_rc_get(&d.rc, "darktable_folder", ""));

        if(ImGui::Button("Import"))
        {
          if(!threads_task_running(impdt_taskid))
          {
            const char* dt_dir = dt_rc_get(&d.rc, "darktable_folder", "");
            if(dt_dir[0])
            {
              d.impdt_abort = 0;
              impdt_text[0] = 0;
              impdt_taskid = ap_import_darktable(dt_dir, impdt_text, &d.impdt_abort);
            }
          }
        }
        ImGui::SameLine();
        if(ImGui::Button("abort") && threads_task_running(impdt_taskid))
          d.impdt_abort = 1;
        ImGui::SameLine();
        ImGui::Text("%s", impdt_text);
      }
    }

//      ImGui::ShowMetricsWindow();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImVec2 window_size = ImGui::GetWindowSize();
    if(window_size.x != d.panel_wd)
    {
      d.panel_wd = window_size.x;
      dt_rc_set_int(&d.rc, "gui/right_panel_width", d.panel_wd);
      d.swapchainrebuild = 1;
    }
    ImGui::End();  // end right panel
  }

//    ImGui::ShowDemoWindow(&show_demo_window);

  // Center view
///*
  {
    ImGuiStyle &style = ImGui::GetStyle();
    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoBackground;
    ImGui::SetNextWindowPos (ImVec2(d.center_x,  d.center_y),  ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(d.center_wd, d.center_ht), ImGuiCond_Always);
    ImGui::Begin("LighttableCenter", 0, window_flags);

    const int border = 0.004 * d.win_width;
    const int width = d.center_wd / d.ipl - border*2 - style.ItemSpacing.x*2;
    const int height = width;
    const int cnt = d.img.collection_cnt;
    const int lines = (cnt+d.ipl-1)/d.ipl;

    if(d.scrollpos == s_scroll_top)
      ImGui::SetScrollY(0.0f);
    else if (d.scrollpos == s_scroll_current)
    {
      if(d.img.current_colid != -1u)
        ImGui::SetScrollY((float)d.img.current_colid/(float)d.img.collection_cnt * ImGui::GetScrollMaxY());
    }
    else if (d.scrollpos == s_scroll_bottom)
      ImGui::SetScrollY(ImGui::GetScrollMaxY());
    d.scrollpos = 0;

    ImGuiListClipper clipper;
    clipper.Begin(lines);
    int first = 1;
    while(clipper.Step())
    {
      // for whatever reason (gauge sizes?) imgui will always pass [0,1) as a first range.
      // we don't want these to trigger a deferred load.
      if(first || clipper.ItemsHeight > 0.0f)
      {
        first = 0;
        dt_thumbnails_load_list(clipper.DisplayStart * d.ipl, clipper.DisplayEnd * d.ipl);
        for(int line=clipper.DisplayStart;line<clipper.DisplayEnd;line++)
        {
          uint32_t i = line * d.ipl;
          for(uint32_t k = 0; k < d.ipl; k++)
          {
            const uint32_t j = i + k;
            ap_image_t *image = &d.img.images[d.img.collection[j]];
            uint32_t tid = image->thumbnail;
            if(tid == 0)
              ap_request_vkdt_thumbnail(image);
            char img[256];
            snprintf(img, sizeof(img), "%s", image->filename);

            if(d.img.collection[j] == d.img.current_imgid)
            {
              ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0, 1.0, 1.0, 1.0));
              ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
            }
            else if(d.img.images[d.img.collection[j]].selected)
            {
              ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6, 0.6, 0.6, 1.0));
              ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8, 0.8, 0.8, 1.0));
            }
            float scale = MIN(
                width / (float)d.thumbs.thumb[tid].wd,
                height / (float)d.thumbs.thumb[tid].ht);
            float w = d.thumbs.thumb[tid].wd * scale;
            float h = d.thumbs.thumb[tid].ht * scale;

            uint32_t ret = ImGui::ThumbnailImage(
                d.img.collection[j],
                d.thumbs.thumb[tid].dset,
                ImVec2(w, h),
                ImVec2(0,0), ImVec2(1,1),
                border,
                ImVec4(0.5f,0.5f,0.5f,1.0f),
                ImVec4(1.0f,1.0f,1.0f,1.0f),
                image->rating,  // rating
                image->labels,  // labels
                img,
                0); // nav_focus

            if(d.img.collection[j] == d.img.current_imgid ||
              d.img.images[d.img.collection[j]].selected)
              ImGui::PopStyleColor(2);

            if(ret)
            {
//              vkdt.wstate.busy += 2;
              if(ImGui::GetIO().KeyCtrl)
              {
                if(d.img.images[d.img.collection[j]].selected)
                  dt_gui_selection_remove(j);
                else
                  dt_gui_selection_add(j);
              }
              else if(ImGui::GetIO().KeyShift)
              { // shift selects ranges
                if(d.img.current_colid != -1u)
                {
                  int a = MIN(d.img.current_colid, j);
                  int b = MAX(d.img.current_colid, j);
                  dt_gui_selection_clear();
                  for(uint32_t i=a;i<=b;i++)
                    dt_gui_selection_add(i);
                }
              }
              else
              { // no modifier, select exactly this image:
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                  ap_gui_image_edit(d.img.collection[j]);
                }
                else
                {
                  dt_gui_selection_clear();
                  dt_gui_selection_add(j);
                }
              }
            }

            if(j + 1 >= cnt) break;
            if(k < d.ipl - 1) ImGui::SameLine();
          }
        }
      }
    }

    ImGui::End(); // end center lighttable
  }
//*/

}

extern "C" int lighttable_enter()
{
  impdt_text[0] = 0;
  impdt_taskid = -1;
  d.impdt_abort = 0;
  d.ipl = dt_rc_get_int(&d.rc, "gui/images_per_line", 6);
  d.img.collection_comp = s_comp_supe;
  dt_filebrowser_init(&vkdtbrowser);
  dt_filebrowser_init(&darktablebrowser);
  ap_navigation_init(&folderbrowser, 0);
  ap_navigation_init(&tagbrowser, 1);
  return 0;
}

extern "C" int lighttable_leave()
{
  return 0;
}
