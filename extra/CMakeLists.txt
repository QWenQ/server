# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1335 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${ZLIB_INCLUDE_DIRS})

# Default install component for the files is Server here
SET(MYSQL_INSTALL_COMPONENT Server)

IF(NOT CMAKE_CROSSCOMPILING OR DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
 ADD_EXECUTABLE(comp_err comp_err.c)
 TARGET_LINK_LIBRARIES(comp_err mysys)
ENDIF()

# Generate mysqld_error.h
# Try not to change its timestamp if not necessary(as touching
# mysqld_error.h results in rebuild of the almost whole server)
# To preserve timestamp, first generate a temp header file, then copy it
# to mysqld_error.h using cmake -E copy_if_different
ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_BINARY_DIR}/include/mysqld_error.h.tmp
   COMMAND comp_err
	--charset=${PROJECT_SOURCE_DIR}/sql/share/charsets
	--out-dir=${CMAKE_BINARY_DIR}/sql/share/
	--header_file=${CMAKE_BINARY_DIR}/include/mysqld_error.h.tmp
	--name_file=${CMAKE_BINARY_DIR}/include/mysqld_ername.h.tmp
	--state_file=${CMAKE_BINARY_DIR}/include/sql_state.h.tmp
	--in_file=${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt
   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/include/mysqld_error.h.tmp ${CMAKE_BINARY_DIR}/include/mysqld_error.h
   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/include/mysqld_ername.h.tmp ${CMAKE_BINARY_DIR}/include/mysqld_ername.h
   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/include/sql_state.h.tmp ${CMAKE_BINARY_DIR}/include/sql_state.h
   DEPENDS comp_err ${PROJECT_SOURCE_DIR}/sql/share/errmsg-utf8.txt)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS
                  ${CMAKE_BINARY_DIR}/include/mysqld_error.h.tmp)

MYSQL_ADD_EXECUTABLE(my_print_defaults my_print_defaults.c COMPONENT Client)
TARGET_LINK_LIBRARIES(my_print_defaults  mysys)

MYSQL_ADD_EXECUTABLE(perror perror.c)
ADD_DEPENDENCIES(perror GenError)
TARGET_LINK_LIBRARIES(perror mysys)

IF(UNIX)
MYSQL_ADD_EXECUTABLE(resolveip resolveip.c)
TARGET_LINK_LIBRARIES(resolveip mysys)
IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  INCLUDE(CheckFunctionExists)
  INCLUDE(CheckLibraryExists)
  MY_SEARCH_LIBS(inet_aton  "nsl;socket;resolv" SOLARIS_NSL)
  TARGET_LINK_LIBRARIES(resolveip ${SOLARIS_NSL})
ENDIF()
ENDIF()

IF(WITH_INNOBASE_STORAGE_ENGINE)
  # Add path to the InnoDB headers
  INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/storage/innobase/include
    ${CMAKE_SOURCE_DIR}/sql)

  # We use the InnoDB code directly in case the code changes.
  ADD_DEFINITIONS("-DUNIV_INNOCHECKSUM")

  SET(INNOBASE_SOURCES
      ../storage/innobase/buf/buf0checksum.cc
      ../storage/innobase/ut/ut0ut.cc
      ../storage/innobase/buf/buf0buf.cc
      ../storage/innobase/page/page0zip.cc
      ../storage/innobase/fil/fil0crypt.cc
      )


  # clang ~16+ with return values being undefined is resolved by basic optimization
  # compiler flags for the function mach_read_from_2 (per MDEV-36316)
  IF(WITH_MSAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
      SET_SOURCE_FILES_PROPERTIES(
          ${INNOBASE_SOURCES}
          innochecksum.cc
          PROPERTIES COMPILE_FLAGS -Og)
  ENDIF()
  MYSQL_ADD_EXECUTABLE(innochecksum innochecksum.cc ${INNOBASE_SOURCES})
  TARGET_LINK_LIBRARIES(innochecksum mysys mysys_ssl)
  ADD_DEPENDENCIES(innochecksum GenError)
ENDIF()

MYSQL_ADD_EXECUTABLE(replace replace.c COMPONENT Client)
TARGET_LINK_LIBRARIES(replace mysys)

IF(UNIX)
  MYSQL_ADD_EXECUTABLE(resolve_stack_dump resolve_stack_dump.c COMPONENT Client)
  TARGET_LINK_LIBRARIES(resolve_stack_dump mysys)

  MYSQL_ADD_EXECUTABLE(mariadb-waitpid mysql_waitpid.c COMPONENT Client)
  TARGET_LINK_LIBRARIES(mariadb-waitpid mysys)

  MYSQL_ADD_EXECUTABLE(mariadbd-safe-helper mysqld_safe_helper.c COMPONENT Server)
  TARGET_LINK_LIBRARIES(mariadbd-safe-helper mysys)
ENDIF()
