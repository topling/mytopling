# Copyright (c) 2026, MyTopling.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# Split the debug info of the installed binaries: each .debug section goes to
# <debug dir>/<path relative to prefix>.debug, the binary is stripped in place
# and gets a .gnu_debuglink, and unpacking the debug tar over the install
# prefix then puts the .debug file where gdb looks for it.
#
# Nothing is done unless STRIP_DEBUG_INFO=1, given as -D or through the
# environment. The script runs as the last step of "cmake --install", and on
# its own for binaries installed outside cmake.
#
# Options, each as -D or through the environment: DEBUG_INFO_PREFIX is the
# install prefix known at install time, DEFAULT_PREFIX otherwise (a plain
# PREFIX is too common a name, it may well be set for unrelated reasons);
# DEBUG_INFO_DIR defaults to <prefix>-debug-info, a sibling, so a tar of the
# install tree carries no debug info unless asked; "cmake --install" passes
# DEBUG_INFO_DEFAULT, the same sibling of the prefix before DESTDIR, so a
# staging root does not swallow the debug info into the package;
# DEBUG_INFO_FILES limits the work to what "cmake --install" installed in this
# run, so nothing else under the prefix is stripped, and unset it is
# everything under bin/ and lib/. DEBUG_INFO_FILES_FILE is the same list, one
# path per line: a full install manifest does not fit in one argument.

IF(NOT DEFINED STRIP_DEBUG_INFO)
  SET(STRIP_DEBUG_INFO "$ENV{STRIP_DEBUG_INFO}")
ENDIF()
IF(NOT STRIP_DEBUG_INFO STREQUAL "1")
  RETURN()
ENDIF()

IF(DEBUG_INFO_PREFIX)
  SET(prefix "${DEBUG_INFO_PREFIX}")
ELSEIF(DEFINED ENV{DEBUG_INFO_PREFIX} AND NOT "$ENV{DEBUG_INFO_PREFIX}" STREQUAL "")
  SET(prefix "$ENV{DEBUG_INFO_PREFIX}")
ELSE()
  SET(prefix "${DEFAULT_PREFIX}")
ENDIF()
GET_FILENAME_COMPONENT(prefix "${prefix}" ABSOLUTE)

IF(NOT OBJCOPY_EXECUTABLE)
  FIND_PROGRAM(OBJCOPY_EXECUTABLE objcopy)
ENDIF()
IF(NOT OBJCOPY_EXECUTABLE)
  MESSAGE(FATAL_ERROR "install-debug-info: need objcopy")
ENDIF()
IF(NOT READELF_EXECUTABLE)
  FIND_PROGRAM(READELF_EXECUTABLE readelf)
ENDIF()
IF(NOT READELF_EXECUTABLE)
  MESSAGE(FATAL_ERROR "install-debug-info: need readelf")
ENDIF()

IF(DEBUG_INFO_DIR)
  SET(debug_info_dir "${DEBUG_INFO_DIR}")
ELSEIF(DEFINED ENV{DEBUG_INFO_DIR} AND NOT "$ENV{DEBUG_INFO_DIR}" STREQUAL "")
  SET(debug_info_dir "$ENV{DEBUG_INFO_DIR}")
ELSEIF(DEBUG_INFO_DEFAULT)
  SET(debug_info_dir "${DEBUG_INFO_DEFAULT}")
ELSE()
  SET(debug_info_dir "${prefix}-debug-info")
ENDIF()
GET_FILENAME_COMPONENT(debug_info_dir "${debug_info_dir}" ABSOLUTE)
IF(DEBUG_INFO_FILES_FILE)
  FILE(STRINGS "${DEBUG_INFO_FILES_FILE}" binaries)
ELSEIF(DEBUG_INFO_FILES)
  SET(binaries ${DEBUG_INFO_FILES})
ELSE()
  FILE(GLOB_RECURSE binaries LIST_DIRECTORIES false
    "${prefix}/bin/*" "${prefix}/lib/*")
ENDIF()
IF(NOT binaries)
  MESSAGE(FATAL_ERROR "install-debug-info: no binaries under ${prefix}")
ENDIF()
FILE(MAKE_DIRECTORY "${debug_info_dir}")

SET(separated 0)
FOREACH(binary ${binaries})
  # A versioned .so symlink must stay a symlink, and the debug dir holds the
  # results of an earlier run; never touch either of them again.
  IF(IS_SYMLINK "${binary}" OR NOT EXISTS "${binary}")
    CONTINUE()
  ENDIF()
  IF(binary MATCHES "\\.debug$")
    CONTINUE()
  ENDIF()
  STRING(FIND "${binary}" "${debug_info_dir}/" debug_dir_pos)
  IF(debug_dir_pos EQUAL 0)
    CONTINUE()
  ENDIF()
  FILE(RELATIVE_PATH relative "${prefix}" "${binary}")
  IF(IS_ABSOLUTE "${relative}" OR relative MATCHES "^\\.\\.")
    MESSAGE(FATAL_ERROR
      "install-debug-info: ${binary} is not under ${prefix}")
  ENDIF()

  # Static archives are gdb-readable as they are, and an .a has .debug_info
  # in every member.
  IF(binary MATCHES "\\.a$")
    CONTINUE()
  ENDIF()
  EXECUTE_PROCESS(COMMAND ${READELF_EXECUTABLE} -S "${binary}"
    OUTPUT_VARIABLE sections RESULT_VARIABLE result ERROR_QUIET)
  IF(result OR NOT sections MATCHES "\\.debug_info")
    CONTINUE()
  ENDIF()

  SET(debug_file "${debug_info_dir}/${relative}.debug")
  GET_FILENAME_COMPONENT(debug_dir "${debug_file}" DIRECTORY)
  FILE(MAKE_DIRECTORY "${debug_dir}")
  MESSAGE(STATUS "install-debug-info: ${binary} -> ${debug_file}")

  EXECUTE_PROCESS(COMMAND ${OBJCOPY_EXECUTABLE} --only-keep-debug
    "${binary}" "${debug_file}" RESULT_VARIABLE result)
  IF(result)
    MESSAGE(FATAL_ERROR
      "install-debug-info: --only-keep-debug failed on ${binary}")
  ENDIF()

  EXECUTE_PROCESS(COMMAND ${OBJCOPY_EXECUTABLE} --strip-debug
    --add-gnu-debuglink=${debug_file} "${binary}" RESULT_VARIABLE result)
  IF(result)
    MESSAGE(FATAL_ERROR "install-debug-info: --strip-debug failed on ${binary}")
  ENDIF()

  MATH(EXPR separated "${separated} + 1")
ENDFOREACH()

MESSAGE(STATUS "install-debug-info: ${separated} file(s) separated")
