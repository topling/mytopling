# Copyright (c) 2026, MyTopling.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# Added last from the top level, so this install rule runs after every other
# one: with STRIP_DEBUG_INFO=1 in the environment a full "cmake --install"
# never leaves the debug sections in the installed binaries, see
# ../install_debug_info.cmake.

INSTALL(CODE "
  IF(\"\$ENV{STRIP_DEBUG_INFO}\" STREQUAL \"1\")
    STRING(REPLACE \";\" \"\\n\" manifest \"\${CMAKE_INSTALL_MANIFEST_FILES}\")
    FILE(WRITE \"${CMAKE_BINARY_DIR}/install_debug_info_files.txt\" \"\${manifest}\")
    EXECUTE_PROCESS(COMMAND \"${CMAKE_COMMAND}\"
      \"-DDEBUG_INFO_PREFIX=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\"
      \"-DDEBUG_INFO_DEFAULT=\${CMAKE_INSTALL_PREFIX}-debug-info\"
      \"-DDEBUG_INFO_FILES_FILE=${CMAKE_BINARY_DIR}/install_debug_info_files.txt\"
      \"-DOBJCOPY_EXECUTABLE=${CMAKE_OBJCOPY}\"
      -P \"${CMAKE_SOURCE_DIR}/cmake/install_debug_info.cmake\"
      RESULT_VARIABLE result)
    IF(NOT result EQUAL 0)
      MESSAGE(FATAL_ERROR \"install-debug-info failed, see above\")
    ENDIF()
  ENDIF()
  " COMPONENT Server)
