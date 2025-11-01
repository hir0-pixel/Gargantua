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

# Find all .comp shader files
file(GLOB SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")

# Output directory for compiled shaders
set(SHADER_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bin")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

# Compile each shader to .spv
set(SHADER_SPV_FILES "")
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SHADER_SPV "${SHADER_OUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
            OUTPUT "${SHADER_SPV}"
            COMMAND "${GLSLC_EXE}" "${SHADER_SRC}" -o "${SHADER_SPV}"
            DEPENDS "${SHADER_SRC}"
            COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )

    list(APPEND SHADER_SPV_FILES "${SHADER_SPV}")
endforeach()

# Create target that depends on all compiled shaders
add_custom_target(compile_shaders ALL DEPENDS ${SHADER_SPV_FILES})

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
message(STATUS "Shaders output: ${SHADER_OUT_DIR}")
message(STATUS "Found ${CMAKE_MATCH_COUNT} shader files to compile")