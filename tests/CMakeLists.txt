# The executable is named cpuaff_tests rather than `test` to avoid
# clashing with CMake's built-in `test` target (alias for ctest).
add_executable(cpuaff_tests test.cpp)
target_link_libraries(cpuaff_tests PRIVATE cpuaff::cpuaff)

add_test(NAME cpuaff COMMAND cpuaff_tests)
