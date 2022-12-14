# Copyright (c) 2015, 2021, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE(GenerateExportHeader)

IF (NOT PROTOBUF_LITE_LIBRARY)
  MESSAGE(FATAL_ERROR "Routing plugin requires protobuf-lite library")
ENDIF()

SET(ROUTING_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mysql_routing.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/destination.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_metadata_cache.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_first_available.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_next_available.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dest_round_robin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/context.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mysql_routing_common.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/connection_container.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing_component.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/channel.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/basic_protocol_splicer.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/classic_protocol_splicer.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/x_protocol_splicer.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/destination_ssl_context.cc
)

SET(ROUTING_PLUGIN_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/routing_plugin.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin_config.cc
)

SET(include_dirs
  ${PROJECT_SOURCE_DIR}/src/metadata_cache/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

SET(system_include_dirs
  ${MYSQLX_GENERATE_DIR}/protobuf_lite
)

# The Plugin
add_harness_plugin(routing
 SOURCES ${ROUTING_PLUGIN_SOURCE_FILES} ${ROUTING_SOURCE_FILES}
 REQUIRES mysql_protocol metadata_cache harness_tls
 DESTINATION "${ROUTER_INSTALL_PLUGINDIR}"
 )
TARGET_INCLUDE_DIRECTORIES(routing
  PRIVATE ${include_dirs}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include
)
TARGET_INCLUDE_DIRECTORIES(routing SYSTEM PRIVATE ${system_include_dirs})
MY_TARGET_INCLUDE_SYSTEM_DIRECTORIES(routing PROTOBUF)
ADD_DEPENDENCIES(routing GenLiteProtos)

TARGET_LINK_LIBRARIES(routing PRIVATE mysqlxmessages_lite ${PROTOBUF_LITE_LIBRARY})

IF(SOLARIS)
  TARGET_LINK_LIBRARIES(routing PRIVATE -lnsl PRIVATE -lsocket)
ENDIF()

# don't install headers until a) a final destination is found and b) API is stable
# FILE(GLOB routing_headers include/mysqlrouter/*.h)
# INSTALL(FILES ${routing_headers}
#   DESTINATION "include/mysql/${HARNESS_NAME}")

GENERATE_EXPORT_HEADER(routing
  EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/mysqlrouter/routing_export.h)

IF(WIN32)
  INSTALL(TARGETS routing 
    RUNTIME DESTINATION "lib"
    COMPONENT Router)
ENDIF()

IF(WITH_UNIT_TESTS)
  ADD_SUBDIRECTORY(tests/)
ENDIF()
