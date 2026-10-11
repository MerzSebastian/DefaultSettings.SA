cmake_minimum_required(VERSION 3.15)
project(DefaultSettingsSA CXX)

# Must be configured as 32-bit:  cmake -S . -B build -A Win32
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "DefaultSettings.SA must be built for Win32 (x86): cmake -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")   # static CRT (/MT)

add_library(DefaultSettingsSA SHARED src/main.cpp src/actions.h src/ini.h src/keys.h)
set_target_properties(DefaultSettingsSA PROPERTIES OUTPUT_NAME "DefaultSettings.SA" SUFFIX ".asi" PREFIX "")
target_link_libraries(DefaultSettingsSA PRIVATE user32 shell32)
target_compile_options(DefaultSettingsSA PRIVATE /W4 /EHsc)

# Host-side self-test (parser, key names, action table) and INI generator.
add_executable(selftest tools/selftest.cpp)
target_compile_options(selftest PRIVATE /W4 /EHsc)
