#set:
# GLib_INCLUDE_DIRS
# GLib_LIBRARY
# GLib_VERSION

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLib REQUIRED glib-2.0)

message(STATUS "inc: ${GLib_INCLUDE_DIRS} lib: ${GLib_LIBRARY} version: ${GLib_VERSION}")
