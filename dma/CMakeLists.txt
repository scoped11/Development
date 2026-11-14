cmake_minimum_required(VERSION 3.20)

# Fail before project() so a refused build/ configure does not fill the folder.
get_filename_component(_robrun_bin_name "${CMAKE_BINARY_DIR}" NAME)
string(TOLOWER "${_robrun_bin_name}" _robrun_bin_name_lower)
if(_robrun_bin_name_lower STREQUAL "build")
    if(NOT ROBRUN_I_APPROVE)
        message(FATAL_ERROR
            "Refusing to configure Development/build/. "
            "Use Development/debug for normal compiles. "
            "Populate build/ only with: dma/scripts/build-windows.ps1 -Section Build -IApprove")
    endif()
endif()

project(RobRunDMA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ROBRUN_I_APPROVE "Kyle explicitly approved writing Development/build/" OFF)

if(_robrun_bin_name_lower STREQUAL "build")
    set(ROBRUN_BUILD_CHANNEL "build")
else()
    set(ROBRUN_BUILD_CHANNEL "debug")
endif()

# Put RobRunDMA.exe + runtime DLLs in debug/ or build/, not a Release subfolder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})

# =============================================================================
# Source files
# =============================================================================
set(SOURCES
    src/main.cpp
    src/core/memory.cpp
    src/engine/engine.cpp
    src/config/config.cpp
    src/config/kmbox_identity.cpp
    src/render/renderer.cpp
    src/render/splash.cpp
    src/render/menu.cpp
    src/features/esp.cpp
    src/features/skeleton_pose.cpp
    src/features/radar.cpp
    src/features/aimbot.cpp
    src/core/kmbox_usb.cpp
    src/core/kmbox_ac.cpp
)

set(HEADERS
    src/pch.h
    src/math/vector.h
    src/core/memory.h
    src/core/scatter.h
    src/engine/offsets.h
    src/engine/engine.h
    src/config/config.h
    src/render/renderer.h
    src/render/splash.h
    src/render/menu.h
    src/features/esp.h
    src/features/radar.h
    src/features/aimbot.h
    src/core/kmbox_usb.h
)

# =============================================================================
# ImGui source files
# =============================================================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

# =============================================================================
# Executable
# =============================================================================
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

# =============================================================================
# Include directories
# =============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/includes
    ${CMAKE_SOURCE_DIR}/external
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ROBRUN_BUILD_CHANNEL="${ROBRUN_BUILD_CHANNEL}"
)

# =============================================================================
# Library directories
# =============================================================================
target_link_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/libs
)

# =============================================================================
# Link libraries
# =============================================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    windowscodecs
    ole32
    winmm
    vmm
    leechcore
    setupapi
    shell32
)

# =============================================================================
# Post-build: Copy runtime DLLs to output directory
# =============================================================================
file(GLOB RUNTIME_DLLS "${CMAKE_SOURCE_DIR}/libs/runtime/*.dll")
file(GLOB RUNTIME_DB "${CMAKE_SOURCE_DIR}/libs/runtime/*.db")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RUNTIME_DLLS} ${RUNTIME_DB}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/loading_animation.gif"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/loading_animation.gif"
    COMMENT "Copying runtime DLLs, config.json, and splash assets"
)
