# where to find glfw for the gui:
MYAP_GLFW_CFLAGS=$(shell pkg-config --cflags glfw3)
MYAP_GLFW_LDFLAGS=$(shell pkg-config --libs glfw3)
export MYAP_GLFW_CFLAGS MYAP_GLFW_LDFLAGS

# compiler config
CC=clang
CXX=clang++
GLSLC=glslangValidator

# optimised flags, you may want to use -march=x86-64 for distro builds:
OPT_CFLAGS=-Wall -pipe -O3 -march=native -DNDEBUG
OPT_LDFLAGS=-s
AR=ar


export CC CXX GLSLC OPT_CFLAGS OPT_LDFLAGS AR
