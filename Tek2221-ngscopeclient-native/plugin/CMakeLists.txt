cmake_minimum_required(VERSION 3.20)
project(Tek2221NativePlugin VERSION 0.6.0 LANGUAGES CXX)
if(NOT WIN32 OR NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Use 64-bit MSYS2 UCRT64 matching the host ngscopeclient build")
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Match the host build type" FORCE)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(NGSCOPECLIENT_SOURCE "" CACHE PATH "Unmodified scopehal-apps checkout used to build the host")
set(NGSCOPECLIENT_BUILD "" CACHE PATH "Matching existing ngscopeclient build directory")
if(NOT EXISTS "${NGSCOPECLIENT_SOURCE}/lib/scopehal/scopehal.h" OR NOT EXISTS "${NGSCOPECLIENT_BUILD}/CMakeCache.txt")
    message(FATAL_ERROR "Set NGSCOPECLIENT_SOURCE and NGSCOPECLIENT_BUILD to the matching host checkout/build")
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(SIGCXX REQUIRED IMPORTED_TARGET sigc++-2.0)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
find_package(yaml-cpp CONFIG REQUIRED)
# Match the MSYS2 host, which deliberately ignores the vendor SDK.
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
find_library(SCOPEHAL_IMPORT NAMES scopehal PATHS "${NGSCOPECLIENT_BUILD}/lib/scopehal" NO_DEFAULT_PATH REQUIRED)
find_library(LOG_IMPORT NAMES log PATHS "${NGSCOPECLIENT_BUILD}/lib/log" NO_DEFAULT_PATH REQUIRED)
set(driver "${CMAKE_CURRENT_SOURCE_DIR}/../driver")
add_library(Tek2221Native MODULE PluginInit.cpp PluginSidebar.cpp Tek2221PluginPanel.cpp
    "${NGSCOPECLIENT_SOURCE}/src/imgui/imgui.cpp"
    "${NGSCOPECLIENT_SOURCE}/src/imgui/imgui_draw.cpp"
    "${NGSCOPECLIENT_SOURCE}/src/imgui/imgui_tables.cpp"
    "${NGSCOPECLIENT_SOURCE}/src/imgui/imgui_widgets.cpp"
    "${driver}/Tek2221Oscilloscope.cpp" "${driver}/Tek2221Native.cpp"
    "${driver}/Tek2221VISA.cpp" "${driver}/Tek2221Controls.cpp" "${driver}/SCPIVISATransport.cpp")
set_target_properties(Tek2221Native PROPERTIES PREFIX "" OUTPUT_NAME "Tek2221Native")
# Keep our implementation distinct even if someone loads it beside an earlier patched host.
target_compile_definitions(Tek2221Native PRIVATE TEK2221_PLUGIN _USE_MATH_DEFINES __USE_MINGW_ANSI_STDIO=1 VK_ENABLE_BETA_EXTENSIONS=1
    Tek2221Oscilloscope=Tek2221PluginOscilloscope SCPIVISATransport=Tek2221PluginVISATransport)
target_include_directories(Tek2221Native PRIVATE "${driver}" "${CMAKE_CURRENT_SOURCE_DIR}" "${NGSCOPECLIENT_SOURCE}/src/imgui"
    "${NGSCOPECLIENT_SOURCE}/lib/scopehal" "${NGSCOPECLIENT_BUILD}/lib/scopehal"
    "${NGSCOPECLIENT_SOURCE}/lib/log" "${NGSCOPECLIENT_SOURCE}/lib/xptools")
target_link_libraries(Tek2221Native PRIVATE "${SCOPEHAL_IMPORT}" "${LOG_IMPORT}" PkgConfig::SIGCXX
    PkgConfig::HIDAPI yaml-cpp::yaml-cpp Vulkan::Loader Vulkan::Headers OpenMP::OpenMP_CXX user32 gdi32 shell32)
target_compile_options(Tek2221Native PRIVATE -Wall -Wextra)
