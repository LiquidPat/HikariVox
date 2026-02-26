cmake_minimum_required(VERSION 3.16)

#Projekt Name
project(HikariVox)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

set(SOURCE_FILES src/main.cpp
        src/simple_logger.cpp
        src/logger.h
        src/vulkan_base/vulkan_base.h
        src/vulkan_base/vulkan_device.cpp)

#FIND SDL3
    add_subdirectory(libs/SDL)
#FIND Vulkan
find_package(Vulkan REQUIRED)

# HikariVox Exe

add_executable(HikariVox ${SOURCE_FILES})
target_link_libraries(HikariVox PRIVATE SDL3::SDL3)
target_include_directories(HikariVox PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(HikariVox PUBLIC ${Vulkan_LIBRARIES})