include(FindPackageHandleStandardArgs)

# Try to find libraries
find_library(cpp_redis_LIBRARIES
  NAMES cpp_redis tacopie
  DOC "cpp_redis libraries"
)
if (cpp_redis_LIBRARIES)
  message(STATUS "Found cpp_redis libraries: \"${cpp_redis_LIBRARIES}\"")
else()
  message(STATUS "Could not find cpp_redis libraries")
endif()

# Try to find headers
find_path(cpp_redis_INCLUDE_DIRS
  NAMES core/client.hpp

  PATH_SUFFIXES cpp_redis
  DOC "cpp_redis C header"
)
if (cpp_redis_INCLUDE_DIRS)
  message(STATUS "Found cpp_redis include path: \"${cpp_redis_INCLUDE_DIRS}\"")
else()
  message(STATUS "Could not find cpp_redis include path")
endif()


set(REDIS_FOUND 1)
find_package_handle_standard_args(cpp_redis DEFAULT_MSG cpp_redis_INCLUDE_DIRS cpp_redis_LIBRARIES)
