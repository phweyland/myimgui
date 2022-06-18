find_package(PkgConfig QUIET)
pkg_check_modules(OSMGpsMap QUIET osmgpsmap-1.2)

# message(STATUS "OSMGpsMap pkg ${OSMGpsMap_LIBRARIES}")

foreach(i ${OSMGpsMap_LIBRARIES})
  find_library(_osmgpsmap_LIBRARY NAMES ${i} HINTS ${OSMGpsMap_LIBRARY_DIRS})
  LIST(APPEND OSMGpsMap_LIBRARY ${_osmgpsmap_LIBRARY})
  unset(_osmgpsmap_LIBRARY CACHE)
endforeach(i)
set(OSMGpsMap_LIBRARIES ${OSMGpsMap_LIBRARY})
unset(OSMGpsMap_LIBRARY CACHE)

# message(STATUS "OSMGpsMap lib ${OSMGpsMap_LIBRARY}")
