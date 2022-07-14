# flat makefile for everything, -j friendly.

.PHONY: all clean

all: myap Makefile

include core/flat.mk
include db/flat.mk
include gui/flat.mk

clean: Makefile
	rm -f myap
	rm -f $(GUI_O) $(CORE_O) $(DB_O)

# GNU_SOURCE for sched_affinity
CFLAGS=-Wall -pipe -I. -D_GNU_SOURCE -std=c11
CXXFLAGS=-Wall -pipe -I. -D_GNU_SOURCE
EXE_CFLAGS=-fPIE
LDFLAGS=
VK_CFLAGS=$(shell pkg-config --cflags vulkan)
VK_LDFLAGS=$(shell pkg-config --libs vulkan)

core/%.o: core/%.c Makefile $(CORE_H) core/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(CORE_CFLAGS) -c $< -o $@

db/%.o: db/%.c Makefile $(DB_H) db/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(DB_CFLAGS) -c $< -o $@

gui/%.o: gui/%.c Makefile $(GUI_H) gui/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

gui/%.o: gui/%.cc Makefile $(GUI_H) gui/flat.mk
	$(CXX) $(CXXFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

../ext/imgui/%.o: ../ext/imgui/%.cpp Makefile
	$(CXX) $(CXXFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

ext/imgui/examples/%.o: ext/imgui/examples/%.cpp Makefile
	$(CXX) $(CXXFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

# main application
# ======================
myap: $(GUI_O) $(CORE_O) $(DB_O) Makefile
	$(CC) $(GUI_O) $(CORE_O) $(DB_O) -fpie -o ../bin/myap \
  	$(VK_LDFLAGS) $(LDFLAGS) $(GUI_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)
