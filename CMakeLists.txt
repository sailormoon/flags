cmake_minimum_required(VERSION 3.20)

project(
  flags
  VERSION 2.0.0
  DESCRIPTION
    "Simple, extensible, header-only C++23 argument parser released into the public domain."
  HOMEPAGE_URL "https://github.com/sailormoon/flags"
  LANGUAGES CXX)

set(FLAGS_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(FLAGS_IS_TOP_LEVEL ON)
endif()

option(FLAGS_INSTALL "Generate install targets for flags" ${FLAGS_IS_TOP_LEVEL})
option(FLAGS_BUILD_TESTS "Build flags unit tests" ${FLAGS_IS_TOP_LEVEL})

add_library(${PROJECT_NAME} INTERFACE)

# Add alias so the project can be used consistently via add_subdirectory,
# FetchContent, or find_package.
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

include(GNUInstallDirs)

# Adding the install interface generator expression makes sure that the include
# files are installed to the proper location (provided by GNUInstallDirs).
target_include_directories(
  ${PROJECT_NAME}
  INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)

if(FLAGS_INSTALL)
  # Locations are provided by GNUInstallDirs.
  install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)

  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

  install(
    EXPORT ${PROJECT_NAME}_Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

  install(
    FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
          "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

  install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if(FLAGS_BUILD_TESTS)
  include(CTest)

  find_package(doctest QUIET)
  if(NOT doctest_FOUND)
    include(FetchContent)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      doctest
      GIT_REPOSITORY https://github.com/doctest/doctest.git
      GIT_TAG v2.5.3
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
  endif()

  if(TARGET doctest)
    get_target_property(_doctest_inc doctest INTERFACE_INCLUDE_DIRECTORIES)
    if(_doctest_inc)
      set_target_properties(
        doctest PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_doctest_inc}")
    endif()
  endif()

  add_executable(flags_test test/flags.cc test/multi_tu.cc)
  target_link_libraries(flags_test PRIVATE flags::flags
                                           doctest::doctest_with_main)
  if(MSVC)
    target_compile_options(flags_test PRIVATE /W4 /WX /permissive- /external:W0)
  else()
    target_compile_options(flags_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  add_test(NAME flags_test COMMAND flags_test)
endif()

if(FLAGS_IS_TOP_LEVEL)
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
  include(CPack)
endif()
