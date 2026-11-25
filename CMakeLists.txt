cmake_minimum_required(VERSION 3.22)
project(CrashForge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- raylib ----
include(FetchContent)
if(ANDROID)
  set(PLATFORM "Android" CACHE STRING "" FORCE)
else()
  set(PLATFORM "Desktop" CACHE STRING "" FORCE)
endif()
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_OBJ OFF CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_GLTF OFF CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RTEXTURES ON CACHE BOOL "" FORCE)

FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

# ---- игра ----
file(GLOB SRC CONFIGURE_DEPENDS src/*.cpp)

if(ANDROID)
  add_library(crashforge SHARED ${SRC})
else()
  add_executable(crashforge ${SRC})
endif()

target_include_directories(crashforge PRIVATE src)
target_link_libraries(crashforge PRIVATE raylib)

if(ANDROID)
  # raylib тянет native_app_glue сам. Если линкер ругается на
  # undefined android_main — раскомментируй блок ниже.
  # target_sources(crashforge PRIVATE
  #   ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
  # target_include_directories(crashforge PRIVATE
  #   ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_libraries(crashforge PRIVATE log android EGL GLESv2 m)
  target_compile_definitions(crashforge PRIVATE PLATFORM_ANDROID)
else()
  target_compile_definitions(crashforge PRIVATE PLATFORM_DESKTOP)
endif()

target_compile_options(crashforge PRIVATE -O3 -ffast-math -fno-exceptions)