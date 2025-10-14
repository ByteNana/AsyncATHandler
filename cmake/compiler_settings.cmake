set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Set build type to Debug for debugging symbols ---
set(CMAKE_BUILD_TYPE Debug)


# In CMakeLists.txt - set default parallel build
if(NOT DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(PROCESSOR_COUNT)
    if(NOT PROCESSOR_COUNT EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${PROCESSOR_COUNT})
        message(STATUS "Setting parallel build level to ${PROCESSOR_COUNT}")
    endif()
endif()
