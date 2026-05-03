cmake_minimum_required(VERSION 3.20)
project(stasis VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Require 64-bit build to match the 64-bit JVM
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Stasis must be built as 64-bit (x64). Set -A x64 or use an x64 generator.")
endif()

file(GLOB_RECURSE IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_win32.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

add_subdirectory(vendor/minhook EXCLUDE_FROM_ALL)

add_library(stasis SHARED
    src/main.cpp
    src/SDK/sdk.cpp
    src/SDK/Minecraft/Minecraft.cpp
    src/SDK/Entity/Entity.cpp
    src/Rendering/Hooking/hooking.cpp
    src/Rendering/Overlay/overlay.cpp
    src/Rendering/ArrayList.cpp
    src/GUI/ClickGUI.cpp
    src/ModuleManager/ModuleManager.cpp
    src/ModuleManager/Modules/Movement/Stasis.cpp
    src/KeybindHandler/KeybindHandler.cpp
    ${IMGUI_SOURCES}
)

# Find Java home for JNI headers
if(NOT DEFINED ENV{JAVA_HOME})
    message(FATAL_ERROR "JAVA_HOME environment variable not set. Point it to your JDK installation (64-bit).")
endif()

target_include_directories(stasis PRIVATE
    src/
    vendor/imgui/
    vendor/imgui/backends/
    $ENV{JAVA_HOME}/include/
    $ENV{JAVA_HOME}/include/win32/
)

target_link_libraries(stasis PRIVATE
    minhook
    $ENV{JAVA_HOME}/lib/jvm.lib
    opengl32.lib
    glu32.lib
    dwmapi.lib
    imm32.lib
)

target_compile_definitions(stasis PRIVATE
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(stasis PROPERTIES
    OUTPUT_NAME "stasis"
    SUFFIX ".dll"
)

# --- Injector executable -------------------------------------------------
# After stasis.dll is built, copy it into the injector source dir so that
# the resource compiler can embed it as a binary resource.
add_custom_command(TARGET stasis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:stasis>"
        "${CMAKE_CURRENT_SOURCE_DIR}/injector/stasis.dll"
    COMMENT "Copying stasis.dll -> injector/ for resource embedding"
)

add_executable(stasis_injector WIN32
    injector/injector.cpp
    injector/injector.rc
)

# Ensure stasis (and its POST_BUILD copy) runs before the injector builds
add_dependencies(stasis_injector stasis)

target_compile_definitions(stasis_injector PRIVATE
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(stasis_injector PROPERTIES
    OUTPUT_NAME "StasisInjector"
)
