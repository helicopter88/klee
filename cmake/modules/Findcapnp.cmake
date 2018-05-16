include(FindPackageHandleStandardArgs)

# Try to find libraries
find_library(capnp_LIBRARIES
  NAMES capnp
  DOC "capnp libraries"
)
if (capnp_LIBRARIES)
  message(STATUS "Found capnp libraries: \"${capnp_LIBRARIES}\"")
else()
  message(STATUS "Could not find capnp libraries")
endif()

# Try to find headers
find_path(capnp_INCLUDE_DIRS
  NAMES c++.capnp.h
  PATH_SUFFIXES capnp
  DOC "capnp C header"
)
if (capnp_INCLUDE_DIRS)
  message(STATUS "Found capnp include path: \"${capnp_INCLUDE_DIRS}\"")
else()
  message(STATUS "Could not find capnp include path")
endif()

set(CAPNP_FOUND 1)
find_package_handle_standard_args(capnp DEFAULT_MSG capnp_INCLUDE_DIRS capnp_LIBRARIES)
