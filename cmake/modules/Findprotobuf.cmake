include(FindPackageHandleStandardArgs)

# Try to find libraries
find_library(protobuf_LIBRARIES
  NAMES protobuf
  DOC "protobuf libraries"
)
if (protobuf_LIBRARIES)
  message(STATUS "Found protobuf libraries: \"${protobuf_LIBRARIES}\"")
else()
  message(STATUS "Could not find protobuf libraries")
endif()

# Try to find headers
find_path(protobuf_INCLUDE_DIRS
  NAMES protobuf/any.pb.h
  PATH_SUFFIXES google
  DOC "protobuf C header"
)
if (protobuf_INCLUDE_DIRS)
  message(STATUS "Found protobuf include path: \"${protobuf_INCLUDE_DIRS}\"")
else()
  message(STATUS "Could not find protobuf include path")
endif()

set(PROTOBUF_FOUND 1)
find_package_handle_standard_args(protobuf DEFAULT_MSG protobuf_INCLUDE_DIRS protobuf_LIBRARIES)
