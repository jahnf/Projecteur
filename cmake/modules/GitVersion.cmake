# GitVersion.cmake - Generate version information using information from git.
# Best suited with git-flow workflows.

# Written for building dcled-hidapi - userland driver for the Dream Cheeky LED Message Board
# Copyright 2018 Jahn Fuchs <github.jahnf@wolke7.net>
# Distributed under the MIT License. See accompanying LICENSE file.

# Definition:
# LAST_TAG_VERSION = latest tagged version (e.g. 1.2.0 for v1.2.0) or 0.0.0 if it does not exist.
#
# Version Number rules:
# - on 'master':  X.Y.Z[-DIST] (using LAST_TAG_VERSION),while DIST should always be 0 on the master branch
# - on 'develop' and other branches : X.Y.Z-ALPHA_FLAG.DIST (using LAST_TAG_VERSION, incrementing Y by 1)
# - on release branches: X.Y.Z-RC_FLAG.DIST (using semver from release branch name
#                                            or XYZ like on develop if not possible)
#                        DIST is either calculated to last tag or to the closest rc-X.Y.Z tag

# * DISTANCE is only added on versions from master branch when != 0
# * On all other branches DISTANCE is always added.
# * All version numbers besides the ones from master have a pre-release identifier set.
# * When printing the version string and the PATCH number is 0 - the patch number is omitted.

### Configuration
set(VERSION_TAG_PREFIX v)     # Should be the same as configured in git-flow
set(VERSION_ALPHA_FLAG alpha) # Pre-release identifier for all builds besides release and hotfix branches
set(VERSION_RC_FLAG rc)       # Pre-release identifier for all builds from release and hotfix branches
set(VERSION_RC_START_TAG_PREFIX "rc-") # If available tags with the given prefix are used for distance calculation on release branches.
set(RC_BRANCH_PREFIX release) # e.g. release/0.2
set(HOTFIX_BRANCH_PREFIX hotfix) # e.g. hotfix/2.0.3

set(_GitVersion_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")

# Get the version information for a directory, sets the following variables
# ${prefix}_VERSION_SUCCESS // 0 on error (e.g. git not found), 1 on success
# ${prefix}_VERSION_MAJOR
# ${prefix}_VERSION_MINOR
# ${prefix}_VERSION_PATCH
# ${prefix}_VERSION_FLAG
# ${prefix}_VERSION_DISTANCE
# ${prefix}_VERSION_SHORTHASH
# ${prefix}_VERSION_FULLHASH
# ${prefix}_VERSION_ISDIRTY // 0 or 1 if tree has local modifications
# ${prefix}_VERSION_STRING // Full version string, e.g. 1.2.3-rc.239
#
# A created version number can be overruled if the following variables are set and the version number is GREATER
# than the dynamically created one.
# - ${prefix}_OR_VERSION_MAJOR
# - ${prefix}_OR_VERSION_MINOR
# - ${prefix}_OR_VERSION_PATCH
function(get_version_info prefix directory)
  set(${prefix}_VERSION_SUCCESS 0 PARENT_SCOPE)
  set(${prefix}_VERSION_MAJOR 0)
  set(${prefix}_VERSION_MINOR 0)
  set(${prefix}_VERSION_PATCH 0)
  set(${prefix}_VERSION_BRANCH unknown PARENT_SCOPE)
  set(${prefix}_VERSION_FLAG unknown)
  set(${prefix}_VERSION_DISTANCE 0)
  set(${prefix}_VERSION_STRING 0.0.0-unknown)
  set(${prefix}_VERSION_ISDIRTY 0 PARENT_SCOPE)
  find_package(Git)
  if(GIT_FOUND)
    # Get the version info from the last tag
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "${VERSION_TAG_PREFIX}[0-9].[0-9]*"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE GIT_TAG_VERSION
      ERROR_VARIABLE error_out
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${directory}
    )
    if(result EQUAL 0)
      if(GIT_TAG_VERSION MATCHES "^${VERSION_TAG_PREFIX}?([0-9]+)\\.([0-9]+)(\\.([0-9]+))?(-([0-9]+))?.*$")
        set(${prefix}_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(${prefix}_VERSION_MINOR ${CMAKE_MATCH_2})
        if(NOT ${CMAKE_MATCH_4} STREQUAL "")
          set(${prefix}_VERSION_PATCH ${CMAKE_MATCH_4})
        endif()
        if(NOT ${CMAKE_MATCH_6} STREQUAL "")
          set(${prefix}_VERSION_DISTANCE ${CMAKE_MATCH_6})
        endif()
      endif()
    else()
      # Count distance
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        RESULT_VARIABLE result
        OUTPUT_VARIABLE GIT_DISTANCE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${directory}
      )
      if(result EQUAL 0)
        set(${prefix}_VERSION_DISTANCE ${GIT_DISTANCE})
      endif()
    endif()

    execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --dirty
      RESULT_VARIABLE result
      OUTPUT_VARIABLE GIT_ALWAYS_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${directory}
    )
    if(result EQUAL 0)
      if(GIT_ALWAYS_VERSION MATCHES "^.*-dirty$")
        set(${prefix}_VERSION_ISDIRTY 1 PARENT_SCOPE)
      endif()
    endif()

    # Check the branch we are on
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      RESULT_VARIABLE result
      OUTPUT_VARIABLE GIT_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${directory}
    )

    if(result EQUAL 0)
      set(${prefix}_VERSION_BRANCH "${GIT_BRANCH}" PARENT_SCOPE)

      # Check for release branch
      string(LENGTH ${RC_BRANCH_PREFIX} PREFIX_LEN)
      string(SUBSTRING ${GIT_BRANCH} 0 ${PREFIX_LEN} COMPARE_PREFIX)
      string(COMPARE EQUAL ${RC_BRANCH_PREFIX} ${COMPARE_PREFIX} ON_RELEASE_BRANCH)
      # Check for hotfix branch
      string(LENGTH ${HOTFIX_BRANCH_PREFIX} PREFIX_LEN)
      string(SUBSTRING ${GIT_BRANCH} 0 ${PREFIX_LEN} COMPARE_PREFIX)
      string(COMPARE EQUAL ${HOTFIX_BRANCH_PREFIX} ${COMPARE_PREFIX} ON_HOTFIX_BRANCH)
      # Check for master branch
      string(COMPARE EQUAL "master" ${GIT_BRANCH} ON_MASTER)

      if(ON_RELEASE_BRANCH)
        set(${prefix}_VERSION_FLAG ${VERSION_RC_FLAG})
        set(RC_VERSION_MAJOR 0)
        set(RC_VERSION_MINOR 0)
        set(RC_VERSION_PATCH 0)
        if(GIT_BRANCH MATCHES "^${RC_BRANCH_PREFIX}.*([0-9]+)\\.([0-9]+)(\\.([0-9]+))?.*$")
          set(RC_VERSION_MAJOR ${CMAKE_MATCH_1})
          set(RC_VERSION_MINOR ${CMAKE_MATCH_2})
          if(NOT ${CMAKE_MATCH_4} STREQUAL "")
            set(RC_VERSION_PATCH ${CMAKE_MATCH_4})
          endif()
        endif()

        if("${RC_VERSION_MAJOR}.${RC_VERSION_MINOR}.${RC_VERSION_PATCH}" VERSION_GREATER
            "${${prefix}_VERSION_MAJOR}.${${prefix}_VERSION_MINOR}.${${prefix}_VERSION_PATCH}")
          set(${prefix}_VERSION_MAJOR ${RC_VERSION_MAJOR})
          set(${prefix}_VERSION_MINOR ${RC_VERSION_MINOR})
          set(${prefix}_VERSION_PATCH ${RC_VERSION_PATCH})
        else()
          # Auto increment minor number, patch = 0
          MATH(EXPR ${prefix}_VERSION_MINOR "${${prefix}_VERSION_MINOR}+1")
          set(${prefix}_VERSION_PATCH 0)
        endif()

        # Try to get distance from last rc start tag
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "${VERSION_RC_START_TAG_PREFIX}[0-9].[0-9]*"
          RESULT_VARIABLE result
          OUTPUT_VARIABLE GIT_RC_TAG_VERSION
          ERROR_VARIABLE error_out
          OUTPUT_STRIP_TRAILING_WHITESPACE
          WORKING_DIRECTORY ${directory}
        )
        if(result EQUAL 0)
          if(GIT_RC_TAG_VERSION MATCHES "^${VERSION_RC_START_TAG_PREFIX}?([0-9]+)\\.([0-9]+)(\\.([0-9]+))?(-([0-9]+))?.*$")
            if(NOT ${CMAKE_MATCH_6} STREQUAL "")
              set(${prefix}_VERSION_DISTANCE ${CMAKE_MATCH_6})
            else()
              set(${prefix}_VERSION_DISTANCE 0)
            endif()
          endif()
        endif()

      elseif(ON_HOTFIX_BRANCH)
        set(${prefix}_VERSION_FLAG ${VERSION_RC_FLAG})
          set(RC_VERSION_MAJOR 0)
          set(RC_VERSION_MINOR 0)
          set(RC_VERSION_PATCH 0)
          if(GIT_BRANCH MATCHES "^${RC_BRANCH_PREFIX}.*([0-9]+)\\.([0-9]+)(\\.([0-9]+))?.*$")
            set(RC_VERSION_MAJOR ${CMAKE_MATCH_1})
            set(RC_VERSION_MINOR ${CMAKE_MATCH_2})
            if(NOT ${CMAKE_MATCH_4} STREQUAL "")
              set(RC_VERSION_PATCH ${CMAKE_MATCH_4})
            endif()
          endif()

          if("${RC_VERSION_MAJOR}.${RC_VERSION_MINOR}.${RC_VERSION_PATCH}" VERSION_GREATER
              "${${prefix}_VERSION_MAJOR}.${${prefix}_VERSION_MINOR}.${${prefix}_VERSION_PATCH}")
            set(${prefix}_VERSION_MAJOR ${RC_VERSION_MAJOR})
            set(${prefix}_VERSION_MINOR ${RC_VERSION_MINOR})
            set(${prefix}_VERSION_PATCH ${RC_VERSION_PATCH})
          else()
            # Auto increment patch number
            MATH(EXPR ${prefix}_VERSION_PATCH "${${prefix}_VERSION_PATCH}+1")
          endif()
      elseif(ON_MASTER)
       set(${prefix}_VERSION_FLAG "")
      endif()
    endif()

    if(NOT ON_MASTER AND NOT ON_RELEASE_BRANCH AND NOT ON_HOTFIX_BRANCH)
      # Auto increment version number, set alpha flag
      MATH(EXPR ${prefix}_VERSION_MINOR "${${prefix}_VERSION_MINOR}+1")
      set(${prefix}_VERSION_PATCH 0)
      set(${prefix}_VERSION_FLAG ${VERSION_ALPHA_FLAG})
    endif()

    # Check if overrule version is greater than dynamically created one
    if("${${prefix}_OR_VERSION_MAJOR}" STREQUAL "")
      set(${prefix}_OR_VERSION_MAJOR 0)
    endif()
    if("${${prefix}_OR_VERSION_MINOR}" STREQUAL "")
      set(${prefix}_OR_VERSION_MINOR 0)
    endif()
    if("${${prefix}_OR_VERSION_PATCH}" STREQUAL "")
      set(${prefix}_OR_VERSION_PATCH 0)
    endif()
    if("${${prefix}_OR_VERSION_MAJOR}.${${prefix}_OR_VERSION_MINOR}.${${prefix}_OR_VERSION_PATCH}" VERSION_GREATER
        "${${prefix}_VERSION_MAJOR}.${${prefix}_VERSION_MINOR}.${${prefix}_VERSION_PATCH}")
      set(${prefix}_VERSION_MAJOR ${${prefix}_OR_VERSION_MAJOR})
      set(${prefix}_VERSION_MINOR ${${prefix}_OR_VERSION_MINOR})
      set(${prefix}_VERSION_PATCH ${${prefix}_OR_VERSION_PATCH})
    endif()

    set(VERSION_STRING "${${prefix}_VERSION_MAJOR}.${${prefix}_VERSION_MINOR}")
    if(NOT ${${prefix}_VERSION_PATCH} EQUAL 0)
      set(VERSION_STRING "${VERSION_STRING}.${${prefix}_VERSION_PATCH}")
    endif()
    if(NOT ON_MASTER OR NOT ${${prefix}_VERSION_DISTANCE} EQUAL 0)
      set(VERSION_STRING "${VERSION_STRING}-${${prefix}_VERSION_FLAG}")
    endif()
    if(NOT ${${prefix}_VERSION_FLAG} STREQUAL "")
      set(VERSION_STRING "${VERSION_STRING}.")
    endif()
    if(NOT ON_MASTER OR (NOT ON_MASTER AND NOT ${${prefix}_VERSION_DISTANCE} EQUAL 0))
      set(VERSION_STRING "${VERSION_STRING}${${prefix}_VERSION_DISTANCE}")
    endif()

    set(${prefix}_VERSION_MAJOR ${${prefix}_VERSION_MAJOR} PARENT_SCOPE)
    set(${prefix}_VERSION_MINOR ${${prefix}_VERSION_MINOR} PARENT_SCOPE)
    set(${prefix}_VERSION_PATCH ${${prefix}_VERSION_PATCH} PARENT_SCOPE)
    set(${prefix}_VERSION_FLAG ${${prefix}_VERSION_FLAG} PARENT_SCOPE)
    set(${prefix}_VERSION_DISTANCE ${${prefix}_VERSION_DISTANCE} PARENT_SCOPE)
    set(${prefix}_VERSION_STRING "${VERSION_STRING}" PARENT_SCOPE)

    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      RESULT_VARIABLE resultSH
      OUTPUT_VARIABLE GIT_SHORT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${directory}
    )
    if(resultSH EQUAL 0)
      set(${prefix}_VERSION_SHORTHASH ${GIT_SHORT_HASH} PARENT_SCOPE)
    else()
      message(WARNING "Could not fetch short version hash.")
    endif()

    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      RESULT_VARIABLE resultFH
      OUTPUT_VARIABLE GIT_FULL_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${directory}
    )
    if(resultFH EQUAL 0)
      set(${prefix}_VERSION_FULLHASH ${GIT_FULL_HASH} PARENT_SCOPE)
    else()
      message(WARNING "Could not fetch full version hash.")
    endif()

    if(resultSH EQUAL 0 AND resultFH EQUAL 0)
      set(${prefix}_VERSION_SUCCESS 1 PARENT_SCOPE)
    endif()
  else()
    message(WARNING "Git not found. Incomplete version information.")
  endif()
endfunction()

# Add version information to a target, header and source file are configured from templates.
#  (GitVersion.h.in and GitVersion.cc.in if no other templates are defined)
# Variables available to input templates
# @TARGET@ = target name given in the function call, converted to C Identifier (e.g. replace '-')-
# @VERSION_MAJOR@, @VERSION_MINOR@, @VERSION_PATCH@, @VERSION_FLAG@, @VERSION_DISTANCE@
# @VERSION_SHORTHASH@, @VERSION_FULLHASH@, @VERSION_STRING@, @VERSION_ISDIRTY, @VERSION_BRANCH@
function(add_version_info_custom_prefix target prefix directory)
  list(LENGTH ARGN NUM_TEMPLATE_ARGS)
  if(NUM_TEMPLATE_ARGS EQUAL 0)
    # Use default templates
    list(APPEND ARGN "${_GitVersion_DIRECTORY}/GitVersion.h.in")
    list(APPEND ARGN "${_GitVersion_DIRECTORY}/GitVersion.cc.in")
  endif()
  string(MAKE_C_IDENTIFIER "${target}" targetid)

  # Set default values, in case sth goes wrong, e.g. we are not inside a git repo
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 0)
  set(VERSION_PATCH 0)
  set(VERSION_FLAG unknown)
  set(VERSION_DISTANCE 0)
  set(VERSION_SHORTHASH unknown)
  set(VERSION_FULLHASH unknown)
  set(VERSION_STRING "0.0-unknown.0")
  set(VERSION_ISDIRTY 0)
  set(VERSION_BRANCH unknown)
  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/version/${targetid}")

  get_version_info(${prefix} "${directory}")
  if(${${prefix}_VERSION_SUCCESS})
    set(VERSION_MAJOR ${${prefix}_VERSION_MAJOR})
    set(VERSION_MINOR ${${prefix}_VERSION_MINOR})
    set(VERSION_PATCH ${${prefix}_VERSION_PATCH})
    set(VERSION_FLAG ${${prefix}_VERSION_FLAG})
    set(VERSION_DISTANCE ${${prefix}_VERSION_DISTANCE})
    set(VERSION_SHORTHASH ${${prefix}_VERSION_SHORTHASH})
    set(VERSION_FULLHASH ${${prefix}_VERSION_FULLHASH})
    set(VERSION_STRING ${${prefix}_VERSION_STRING})
    set(VERSION_ISDIRTY ${${prefix}_VERSION_ISDIRTY})
    set(VERSION_BRANCH ${${prefix}_VERSION_BRANCH})
  else()
    message(WARNING "Error during version retrieval. Incomplete version information!")
  endif()

  set(TARGET ${prefix})
  foreach(template_file ${ARGN})
    if(template_file MATCHES "(.*)(\.in)$")
      get_filename_component(output_basename "${CMAKE_MATCH_1}" NAME)
    else()
      get_filename_component(output_basename "${template_file}" NAME)
    endif()
    set(output_file "${output_dir}/${prefix}-${output_basename}")
    configure_file("${template_file}" "${output_file}")
    list(APPEND output_files "${output_file}")
  endforeach()

  get_target_property(type ${target} TYPE)
  if(type STREQUAL "SHARED_LIBRARY")
     set_target_properties(${target} PROPERTIES SOVERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
     set_property(TARGET ${target} PROPERTY VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
  endif()
  set_property(TARGET ${target} APPEND PROPERTY SOURCES ${output_files})
  target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${output_dir}>)
  message(STATUS "Version info for '${target}': ${VERSION_STRING}")
endfunction()

function(add_version_info target directory)
  string(MAKE_C_IDENTIFIER "${target}" prefix)
  add_version_info_custom_prefix(${target} ${prefix} ${directory} ${ARGN})
endfunction()
