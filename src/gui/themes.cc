#include "imgui.h"
#include <math.h>
extern "C" {
#include "gui.h"
}

inline ImVec4 gamma(ImVec4 in)
{
  // TODO: these values will be interpreted as rec2020 coordinates. do we want that?
  // theme colours are given as float sRGB values in imgui, while we will
  // draw them in linear:
  return ImVec4(pow(in.x, 2.2), pow(in.y, 2.2), pow(in.z, 2.2), in.w);
}

void ImGui_Dark()
{
  ImGui::StyleColorsDark();
}

void ImGui_Classic()
{
  ImGui::StyleColorsClassic();
}

void dark_corporate_style()
{
  ImGuiStyle & style = ImGui::GetStyle();
  ImVec4 * colors = style.Colors;

	/// 0 = FLAT APPEARENCE
	/// 1 = MORE "3D" LOOK
	int is3D = 0;

	colors[ImGuiCol_Text]                   = gamma(ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
	colors[ImGuiCol_TextDisabled]           = gamma(ImVec4(0.40f, 0.40f, 0.40f, 1.00f));
	colors[ImGuiCol_ChildBg]                = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
	colors[ImGuiCol_WindowBg]               = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
	colors[ImGuiCol_PopupBg]                = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
	colors[ImGuiCol_Border]                 = gamma(ImVec4(0.12f, 0.12f, 0.12f, 0.71f));
	colors[ImGuiCol_BorderShadow]           = gamma(ImVec4(1.00f, 1.00f, 1.00f, 0.06f));
	colors[ImGuiCol_FrameBg]                = gamma(ImVec4(0.42f, 0.42f, 0.42f, 0.54f));
	colors[ImGuiCol_FrameBgHovered]         = gamma(ImVec4(0.42f, 0.42f, 0.42f, 0.40f));
	colors[ImGuiCol_FrameBgActive]          = gamma(ImVec4(0.56f, 0.56f, 0.56f, 0.67f));
	colors[ImGuiCol_TitleBg]                = gamma(ImVec4(0.19f, 0.19f, 0.19f, 1.00f));
	colors[ImGuiCol_TitleBgActive]          = gamma(ImVec4(0.22f, 0.22f, 0.22f, 1.00f));
	colors[ImGuiCol_TitleBgCollapsed]       = gamma(ImVec4(0.17f, 0.17f, 0.17f, 0.90f));
	colors[ImGuiCol_MenuBarBg]              = gamma(ImVec4(0.335f, 0.335f, 0.335f, 1.000f));
	colors[ImGuiCol_ScrollbarBg]            = gamma(ImVec4(0.24f, 0.24f, 0.24f, 0.53f));
	colors[ImGuiCol_ScrollbarGrab]          = gamma(ImVec4(0.41f, 0.41f, 0.41f, 1.00f));
	colors[ImGuiCol_ScrollbarGrabHovered]   = gamma(ImVec4(0.52f, 0.52f, 0.52f, 1.00f));
	colors[ImGuiCol_ScrollbarGrabActive]    = gamma(ImVec4(0.76f, 0.76f, 0.76f, 1.00f));
	colors[ImGuiCol_CheckMark]              = gamma(ImVec4(0.65f, 0.65f, 0.65f, 1.00f));
	colors[ImGuiCol_SliderGrab]             = gamma(ImVec4(0.52f, 0.52f, 0.52f, 1.00f));
	colors[ImGuiCol_SliderGrabActive]       = gamma(ImVec4(0.64f, 0.64f, 0.64f, 1.00f));
	colors[ImGuiCol_Button]                 = gamma(ImVec4(0.54f, 0.54f, 0.54f, 0.35f));
	colors[ImGuiCol_ButtonHovered]          = gamma(ImVec4(0.52f, 0.52f, 0.52f, 0.59f));
	colors[ImGuiCol_ButtonActive]           = gamma(ImVec4(0.76f, 0.76f, 0.76f, 1.00f));
	colors[ImGuiCol_Header]                 = gamma(ImVec4(0.38f, 0.38f, 0.38f, 1.00f));
	colors[ImGuiCol_HeaderHovered]          = gamma(ImVec4(0.47f, 0.47f, 0.47f, 1.00f));
	colors[ImGuiCol_HeaderActive]           = gamma(ImVec4(0.76f, 0.76f, 0.76f, 0.77f));
	colors[ImGuiCol_Separator]              = gamma(ImVec4(0.000f, 0.000f, 0.000f, 0.137f));
	colors[ImGuiCol_SeparatorHovered]       = gamma(ImVec4(0.700f, 0.671f, 0.600f, 0.290f));
	colors[ImGuiCol_SeparatorActive]        = gamma(ImVec4(0.702f, 0.671f, 0.600f, 0.674f));
	colors[ImGuiCol_ResizeGrip]             = gamma(ImVec4(0.26f, 0.59f, 0.98f, 0.25f));
	colors[ImGuiCol_ResizeGripHovered]      = gamma(ImVec4(0.26f, 0.59f, 0.98f, 0.67f));
	colors[ImGuiCol_ResizeGripActive]       = gamma(ImVec4(0.26f, 0.59f, 0.98f, 0.95f));
	colors[ImGuiCol_PlotLines]              = gamma(ImVec4(0.61f, 0.61f, 0.61f, 1.00f));
	colors[ImGuiCol_PlotLinesHovered]       = gamma(ImVec4(1.00f, 0.43f, 0.35f, 1.00f));
	colors[ImGuiCol_PlotHistogram]          = gamma(ImVec4(0.90f, 0.70f, 0.00f, 1.00f));
	colors[ImGuiCol_PlotHistogramHovered]   = gamma(ImVec4(1.00f, 0.60f, 0.00f, 1.00f));
	colors[ImGuiCol_TextSelectedBg]         = gamma(ImVec4(0.73f, 0.73f, 0.73f, 0.35f));
	colors[ImGuiCol_ModalWindowDimBg]       = gamma(ImVec4(0.80f, 0.80f, 0.80f, 0.35f));
	colors[ImGuiCol_DragDropTarget]         = gamma(ImVec4(1.00f, 1.00f, 0.00f, 0.90f));
	colors[ImGuiCol_NavHighlight]           = gamma(ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
	colors[ImGuiCol_NavWindowingHighlight]  = gamma(ImVec4(1.00f, 1.00f, 1.00f, 0.70f));
	colors[ImGuiCol_NavWindowingDimBg]      = gamma(ImVec4(0.80f, 0.80f, 0.80f, 0.20f));

	style.PopupRounding = 3;

	style.ScrollbarSize = 18;

	style.WindowBorderSize = 1;
	style.ChildBorderSize  = 1;
	style.PopupBorderSize  = 1;
	style.FrameBorderSize  = is3D;

	style.WindowRounding    = 0;
	style.ChildRounding     = 0;
	style.FrameRounding     = 0;
	style.ScrollbarRounding = 2;
	style.GrabRounding      = 3;

  style.TabBorderSize = is3D;
  style.TabRounding   = 3;

#ifdef IMGUI_HAS_DOCK
  colors[ImGuiCol_DockingPreview]     = gamma(ImVec4(0.85f, 0.85f, 0.85f, 0.28f));
  colors[ImGuiCol_DockingEmptyBg]     = gamma(ImVec4(0.38f, 0.38f, 0.38f, 1.00f));
  if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
  {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }
#endif

  colors[ImGuiCol_Tab]                = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
  colors[ImGuiCol_TabHovered]         = gamma(ImVec4(0.40f, 0.40f, 0.40f, 1.00f));
  colors[ImGuiCol_TabActive]          = gamma(ImVec4(0.33f, 0.33f, 0.33f, 1.00f));
  colors[ImGuiCol_TabUnfocused]       = gamma(ImVec4(0.25f, 0.25f, 0.25f, 1.00f));
  colors[ImGuiCol_TabUnfocusedActive] = gamma(ImVec4(0.33f, 0.33f, 0.33f, 1.00f));
}

void grey_photoshop_style()
{
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	colors[ImGuiCol_Text]                   = gamma (ImVec4(1.000f, 1.000f, 1.000f, 1.000f));
	colors[ImGuiCol_TextDisabled]           = gamma (ImVec4(0.500f, 0.500f, 0.500f, 1.000f));
	colors[ImGuiCol_WindowBg]               = gamma (ImVec4(0.180f, 0.180f, 0.180f, 1.000f));
	colors[ImGuiCol_ChildBg]                = gamma (ImVec4(0.280f, 0.280f, 0.280f, 0.000f));
	colors[ImGuiCol_PopupBg]                = gamma (ImVec4(0.313f, 0.313f, 0.313f, 1.000f));
	colors[ImGuiCol_Border]                 = gamma (ImVec4(0.266f, 0.266f, 0.266f, 1.000f));
	colors[ImGuiCol_BorderShadow]           = gamma (ImVec4(0.000f, 0.000f, 0.000f, 0.000f));
	colors[ImGuiCol_FrameBg]                = gamma (ImVec4(0.160f, 0.160f, 0.160f, 1.000f));
	colors[ImGuiCol_FrameBgHovered]         = gamma (ImVec4(0.200f, 0.200f, 0.200f, 1.000f));
	colors[ImGuiCol_FrameBgActive]          = gamma (ImVec4(0.280f, 0.280f, 0.280f, 1.000f));
	colors[ImGuiCol_TitleBg]                = gamma (ImVec4(0.148f, 0.148f, 0.148f, 1.000f));
	colors[ImGuiCol_TitleBgActive]          = gamma (ImVec4(0.148f, 0.148f, 0.148f, 1.000f));
	colors[ImGuiCol_TitleBgCollapsed]       = gamma (ImVec4(0.148f, 0.148f, 0.148f, 1.000f));
	colors[ImGuiCol_MenuBarBg]              = gamma (ImVec4(0.195f, 0.195f, 0.195f, 1.000f));
	colors[ImGuiCol_ScrollbarBg]            = gamma (ImVec4(0.160f, 0.160f, 0.160f, 1.000f));
	colors[ImGuiCol_ScrollbarGrab]          = gamma (ImVec4(0.277f, 0.277f, 0.277f, 1.000f));
	colors[ImGuiCol_ScrollbarGrabHovered]   = gamma (ImVec4(0.300f, 0.300f, 0.300f, 1.000f));
	colors[ImGuiCol_ScrollbarGrabActive]    = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_CheckMark]              = gamma (ImVec4(1.000f, 1.000f, 1.000f, 1.000f));
	colors[ImGuiCol_SliderGrab]             = gamma (ImVec4(0.391f, 0.391f, 0.391f, 1.000f));
	colors[ImGuiCol_SliderGrabActive]       = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_Button]                 = gamma (ImVec4(1.000f, 1.000f, 1.000f, 0.000f));
	colors[ImGuiCol_ButtonHovered]          = gamma (ImVec4(1.000f, 1.000f, 1.000f, 0.156f));
	colors[ImGuiCol_ButtonActive]           = gamma (ImVec4(1.000f, 1.000f, 1.000f, 0.391f));
	colors[ImGuiCol_Header]                 = gamma (ImVec4(0.313f, 0.313f, 0.313f, 1.000f));
	colors[ImGuiCol_HeaderHovered]          = gamma (ImVec4(0.469f, 0.469f, 0.469f, 1.000f));
	colors[ImGuiCol_HeaderActive]           = gamma (ImVec4(0.469f, 0.469f, 0.469f, 1.000f));
	colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered]       = gamma (ImVec4(0.391f, 0.391f, 0.391f, 1.000f));
	colors[ImGuiCol_SeparatorActive]        = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_ResizeGrip]             = gamma (ImVec4(1.000f, 1.000f, 1.000f, 0.250f));
	colors[ImGuiCol_ResizeGripHovered]      = gamma (ImVec4(1.000f, 1.000f, 1.000f, 0.670f));
	colors[ImGuiCol_ResizeGripActive]       = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_Tab]                    = gamma (ImVec4(0.098f, 0.098f, 0.098f, 1.000f));
	colors[ImGuiCol_TabHovered]             = gamma (ImVec4(0.352f, 0.352f, 0.352f, 1.000f));
	colors[ImGuiCol_TabActive]              = gamma (ImVec4(0.195f, 0.195f, 0.195f, 1.000f));
	colors[ImGuiCol_TabUnfocused]           = gamma (ImVec4(0.098f, 0.098f, 0.098f, 1.000f));
	colors[ImGuiCol_TabUnfocusedActive]     = gamma (ImVec4(0.195f, 0.195f, 0.195f, 1.000f));
#ifdef IMGUI_HAS_DOCK
	colors[ImGuiCol_DockingPreview]         = gamma (ImVec4(1.000f, 0.391f, 0.000f, 0.781f));
	colors[ImGuiCol_DockingEmptyBg]         = gamma (ImVec4(0.180f, 0.180f, 0.180f, 1.000f));
#endif
	colors[ImGuiCol_PlotLines]              = gamma (ImVec4(0.469f, 0.469f, 0.469f, 1.000f));
	colors[ImGuiCol_PlotLinesHovered]       = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_PlotHistogram]          = gamma (ImVec4(0.586f, 0.586f, 0.586f, 1.000f));
	colors[ImGuiCol_PlotHistogramHovered]   = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_TextSelectedBg]         = gamma (ImVec4(1.000f, 1.000f, 1.000f, 0.156f));
	colors[ImGuiCol_DragDropTarget]         = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_NavHighlight]           = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_NavWindowingHighlight]  = gamma (ImVec4(1.000f, 0.391f, 0.000f, 1.000f));
	colors[ImGuiCol_NavWindowingDimBg]      = gamma (ImVec4(0.000f, 0.000f, 0.000f, 0.586f));
	colors[ImGuiCol_ModalWindowDimBg]       = gamma (ImVec4(0.000f, 0.000f, 0.000f, 0.586f));

	style->ChildRounding = 4.0f;
	style->FrameBorderSize = 1.0f;
	style->FrameRounding = 2.0f;
	style->GrabMinSize = 7.0f;
	style->PopupRounding = 2.0f;
	style->ScrollbarRounding = 12.0f;
	style->ScrollbarSize = 13.0f;
	style->TabBorderSize = 1.0f;
	style->TabRounding = 0.0f;
	style->WindowRounding = 4.0f;
}


void (*themes_set[])() = {
  &ImGui_Dark,
  &ImGui_Classic,
  &dark_corporate_style,
  &grey_photoshop_style
};

const char *themes_name[] = {
  "ImGui Dark",
  "ImGui Classic",
  "dark corporate",
  "grey photoshop",
  NULL
};
