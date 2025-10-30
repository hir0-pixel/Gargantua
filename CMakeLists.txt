cmake_minimum_required(VERSION 3.20)
project(Gargantua VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Explicitly set Vulkan SDK path
set(VULKAN_SDK "C:/VulkanSDK/1.4.328.1")
set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/Include")
set(Vulkan_LIBRARY "${VULKAN_SDK}/Lib/vulkan-1.lib")
set(Vulkan_FOUND TRUE)

# Find GLFW via vcpkg
find_package(glfw3 REQUIRED)

# Source files
set(SOURCES src/main.cpp)

# Create executable
add_executable(gargantua ${SOURCES})

# Include directories
target_include_directories(gargantua PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(gargantua PRIVATE
        ${Vulkan_LIBRARY}
        glfw
)

# MSVC specific options
if(MSVC)
    target_compile_options(gargantua PRIVATE /W4)
endif()

# Print debug info
message(STATUS "Vulkan Include: ${Vulkan_INCLUDE_DIR}")
message(STATUS "Vulkan Library: ${Vulkan_LIBRARY}")