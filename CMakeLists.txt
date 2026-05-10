cmake_minimum_required(VERSION 3.21)

project(cpuaff
    VERSION 2.0.0
    DESCRIPTION "Header-only Linux CPU affinity library"
    HOMEPAGE_URL "https://github.com/rodgert/cpuaff"
    LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
        "cpuaff requires Linux. Use v1.0.6-htaa.1 or earlier for "
        "macOS/BSD/Windows support via the now-removed null_impl and "
        "hwloc_impl backends.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------

add_library(cpuaff INTERFACE)
add_library(cpuaff::cpuaff ALIAS cpuaff)

target_include_directories(cpuaff INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Phase 4 will bump this to cxx_std_20 along with the API modernization.
target_compile_features(cpuaff INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------
# Examples and tests — only when this is the top-level project
# ---------------------------------------------------------------------------

if(PROJECT_IS_TOP_LEVEL)
    option(CPUAFF_BUILD_EXAMPLES "Build cpuaff examples" ON)
    option(CPUAFF_BUILD_TESTS    "Build cpuaff tests"    ON)

    if(CPUAFF_BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()

    if(CPUAFF_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

# ---------------------------------------------------------------------------
# Install rules + package config + pkg-config
# ---------------------------------------------------------------------------

install(DIRECTORY include/cpuaff
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")

install(TARGETS cpuaff EXPORT cpuaffTargets)

install(EXPORT cpuaffTargets
    FILE cpuaffTargets.cmake
    NAMESPACE cpuaff::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpuaff)

configure_package_config_file(
    cmake/cpuaffConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/cpuaffConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpuaff)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cpuaffConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cpuaffConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cpuaffConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpuaff)

# pkg-config: prefer a relocatable install where the .pc file resolves
# its own paths via ${pcfiledir}. If the user pinned an absolute
# CMAKE_INSTALL_INCLUDEDIR we honour that verbatim.
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PKGCONFIG_INCLUDEDIR_TEMPLATE "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PKGCONFIG_INCLUDEDIR_TEMPLATE "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
configure_file(cpuaff.pc.in cpuaff.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cpuaff.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
