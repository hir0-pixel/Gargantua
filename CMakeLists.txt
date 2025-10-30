cmake_minimum_required(VERSION 3.20)
project(Gargantua VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Vulkan SDK (manual hint)
set(VULKAN_SDK "C:/VulkanSDK/1.4.328.1")
set(Vulkan_INCLUDE_DIR "${VULKAN_SDK}/Include")
set(Vulkan_LIBRARY "${VULKAN_SDK}/Lib/vulkan-1.lib")
set(Vulkan_FOUND TRUE)

find_package(glfw3 REQUIRED)

# ---- Shader compilation ----
set(GLSLC_EXE "${VULKAN_SDK}/Bin/glslc.exe")
if(NOT EXISTS "${GLSLC_EXE}")
    message(FATAL_ERROR "glslc not found at ${GLSLC_EXE}")
endif()

set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders/test.comp")
set(SHADER_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")
set(SHADER_SPV "${SHADER_OUT_DIR}/test.comp.spv")

add_custom_command(
        OUTPUT "${SHADER_SPV}"
        COMMAND "${GLSLC_EXE}" "${SHADER_SRC}" -o "${SHADER_SPV}"
        DEPENDS "${SHADER_SRC}"
        COMMENT "Compiling compute shader: test.comp -> test.comp.spv"
)

add_custom_target(compile_shaders ALL DEPENDS "${SHADER_SPV}")

# Pass absolute shader directory to the app
add_compile_definitions(GARGANTUA_SHADER_DIR="${SHADER_OUT_DIR}")

# Sources
set(SOURCES
        src/main.cpp
        src/core/window.cpp
        src/renderer/vulkan_context.cpp
        src/renderer/swapchain.cpp
        src/renderer/compute_pipeline.cpp
)

add_executable(gargantua ${SOURCES})
add_dependencies(gargantua compile_shaders)

target_include_directories(gargantua PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(gargantua PRIVATE
        ${Vulkan_LIBRARY}
        glfw
)

if(MSVC)
    target_compile_options(gargantua PRIVATE /W4)
endif()

message(STATUS "Vulkan Include: ${Vulkan_INCLUDE_DIR}")
message(STATUS "Vulkan Library: ${Vulkan_LIBRARY}")
message(STATUS "Shaders out: ${SHADER_OUT_DIR}")
