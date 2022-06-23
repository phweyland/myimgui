#This module will set the following variables if found:

# Sqlite3_INCLUDE_DIRS
#  where to find sqlite3.h, etc.
# Sqlite3_LIBRARIES
#  the libraries to link against to use Sqlite3.
# Sqlite3_VERSION
#  version of the Sqlite3 library found
# Sqlite3_FOUND
#  TRUE if found

# Look for the necessary header
find_path(Sqlite3_INCLUDE_DIR NAMES sqlite3.h)
mark_as_advanced(Sqlite3_INCLUDE_DIR)

# Look for the necessary library
find_library(Sqlite3_LIBRARY NAMES sqlite3 sqlite)
mark_as_advanced(Sqlite3_LIBRARY)

# Extract version information from the header file
if(Sqlite3_INCLUDE_DIR)
    file(STRINGS ${Sqlite3_INCLUDE_DIR}/sqlite3.h _ver_line
         REGEX "^#define SQLITE_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           Sqlite3_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

find_package_handle_standard_args(Sqlite3
    REQUIRED_VARS Sqlite3_INCLUDE_DIR Sqlite3_LIBRARY
    VERSION_VAR Sqlite3_VERSION)

# Create the imported target
if(Sqlite3_FOUND)
    set(Sqlite3_INCLUDE_DIRS ${Sqlite3_INCLUDE_DIR})
    set(Sqlite3_LIBRARIES ${Sqlite3_LIBRARY})
endif()

#message(STATUS "Sqlite3 dir ${Sqlite3_INCLUDE_DIRS} - lib ${Sqlite3_LIBRARIES}" - version ${Sqlite3_VERSION})
