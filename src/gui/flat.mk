GUI_O=gui/gui.o\
      gui/render.o\
      main.o\
      gui/themes.o\
			gui/widget_filebrowser.o\
			gui/widget_navigation.o\
      ../ext/imgui/imgui.o\
      ../ext/imgui/imgui_draw.o\
      ../ext/imgui/imgui_widgets.o\
      ../ext/imgui/imgui_tables.o\
      ../ext/imgui/backends/imgui_impl_vulkan.o\
      ../ext/imgui/backends/imgui_impl_glfw.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/themes.hh\
      gui/widget_filebrowser.hh\
			gui/widget_navigation.hh\
			gui/widget_thumbnail.hh\
      core/fs.h
GUI_CFLAGS=$(MYAP_GLFW_CFLAGS) -I../ext/imgui -I../ext/imgui/backends/
GUI_LDFLAGS=-ldl $(MYAP_GLFW_LDFLAGS) -lm -lstdc++


#main.o:core/signal.h
