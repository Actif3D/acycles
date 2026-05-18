# SPDX-FileCopyrightText: 2026
#
# SPDX-License-Identifier: Apache-2.0

# Minimal OpenColorIO finder for distributions whose exported CMake package
# computes an invalid install prefix.

find_path(OPENCOLORIO_INCLUDE_DIR
  NAMES OpenColorIO/OpenColorIO.h
)

find_library(OPENCOLORIO_LIBRARY
  NAMES OpenColorIO
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenColorIO
  REQUIRED_VARS OPENCOLORIO_LIBRARY OPENCOLORIO_INCLUDE_DIR
)

if(OpenColorIO_FOUND)
  set(OPENCOLORIO_INCLUDE_DIRS "${OPENCOLORIO_INCLUDE_DIR}")
  set(OPENCOLORIO_LIBRARIES "${OPENCOLORIO_LIBRARY}")

  if(NOT TARGET OpenColorIO::OpenColorIO)
    add_library(OpenColorIO::OpenColorIO UNKNOWN IMPORTED)
    set_target_properties(OpenColorIO::OpenColorIO PROPERTIES
      IMPORTED_LOCATION "${OPENCOLORIO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENCOLORIO_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(OPENCOLORIO_INCLUDE_DIR OPENCOLORIO_LIBRARY)
