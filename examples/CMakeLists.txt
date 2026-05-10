foreach(example IN ITEMS list_cpus simple_affinity affinity_stack)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE cpuaff::cpuaff)
endforeach()
