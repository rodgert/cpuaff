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

# pthread_*affinity_np requires the pthreads runtime. On modern glibc
# (≥ 2.34) the symbols are folded into libc and no link flag is
# strictly needed; on older glibc / musl / etc. -lpthread is required.
# Threads::Threads abstracts the platform variation and propagates
# transitively to consumers via the cpuaff::cpuaff INTERFACE target.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------

add_library(cpuaff INTERFACE)
add_library(cpuaff::cpuaff ALIAS cpuaff)

target_include_directories(cpuaff INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# C++20 baseline (Phase 4). std::expected is preferred when the
# consumer's toolchain ships it (libstdc++ ≥ 12 or libc++ ≥ 16 in
# C++23 mode); otherwise the minimal polyfill in
# include/cpuaff/detail/expected.hpp takes over transparently.
target_compile_features(cpuaff INTERFACE cxx_std_20)

target_link_libraries(cpuaff INTERFACE Threads::Threads)

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

# ---------------------------------------------------------------------------
# Optional documentation target. Driven by doxygen_add_docs(); runs only if
# Doxygen is installed. Output lands in ${CMAKE_BINARY_DIR}/docs/html so it
# doesn't collide with the upstream Jekyll site living under docs/.
# ---------------------------------------------------------------------------

if(PROJECT_IS_TOP_LEVEL)
    find_package(Doxygen QUIET)
    if(Doxygen_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_QUIET YES)
        # Fail the build on broken \refs, missing \param tags, etc.
        # `FAIL_ON_WARNINGS` accumulates the warnings then errors out at
        # the end so we see the full list rather than only the first.
        set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
        set(DOXYGEN_HAVE_DOT NO)

        # detail/expected.hpp has two mutually-exclusive code branches
        # (alias-to-std::expected vs in-tree polyfill). Pin doxygen to
        # the polyfill branch by predefining CPUAFF_HAS_STD_EXPECTED=0,
        # so the polyfill classes (expected / unexpected /
        # bad_expected_access) are what gets documented — the alias
        # branch is a trivial `using` chain to <expected> which is
        # documented at cppreference. Without this pin doxygen would
        # see both branches under EXTRACT_ALL and emit redefinition
        # warnings, which FAIL_ON_WARNINGS would then surface.
        set(DOXYGEN_ENABLE_PREPROCESSING YES)
        set(DOXYGEN_MACRO_EXPANSION YES)
        set(DOXYGEN_PREDEFINED "CPUAFF_HAS_STD_EXPECTED=0")

        doxygen_add_docs(cpuaff_docs
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            COMMENT "Generating cpuaff API documentation")
    endif()
endif()
