# GLOWTIDE - one CMake tree for the Windows dev build and the Android libmain.so (TECH_SPEC 2.1).
# Desktop: scripts/build-desktop.ps1 (native CMake 4.x, Ninja, llvm-mingw clang).
# Android: android/ Gradle project points externalNativeBuild here (SDK CMake 3.22.1, NDK r29).
# Browser: scripts/build-web.ps1 (Emscripten: WebAssembly and WebGL 2, the page from web/shell.html).

cmake_minimum_required(VERSION 3.22)
project(glowtide VERSION 0.2.0 LANGUAGES C CXX)  # (android/app/build.gradle versionName is kept the same)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Sanitizer build of the tests and the headless tool (P6): no counted operator new, dynamic link.
option(GT_SANITIZE "AddressSanitizer + UBSan build of the tests and the headless tool" OFF)
# Development builds (Debug, RelWithDebInfo) keep the dev-only tools such as the level stepper; Release drops them.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_definitions(GT_DEV=1)
endif()
set(GT_STATIC_LINK -static)
if(GT_SANITIZE)
  add_compile_definitions(GT_SANITIZE=1)
  set(GT_STATIC_LINK "")
endif()

set(GT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GT_TP ${GT_ROOT}/third_party)
set(GT_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GT_GEN})
if(NOT EXISTS ${GT_TP}/SDL/CMakeLists.txt)
  message(FATAL_ERROR "third_party/SDL is missing - run scripts/fetch-deps.ps1")
endif()

# Owner's rule: at most 4 parallel jobs, also for the Ninja that Gradle starts.
set(GT_MAX_JOBS 4 CACHE STRING "Parallel compile and link jobs (Ninja job pool)")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS gt_jobs=${GT_MAX_JOBS})
set(CMAKE_JOB_POOL_COMPILE gt_jobs)
set(CMAKE_JOB_POOL_LINK gt_jobs)

if(ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-g)  # unstripped libraries are kept beside the release APK
endif()

# ---- SDL3: static on the desktop, shared on Android ---------------------------------------------------------
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
# Subsystems the game never uses (the Android Java glue's own ones, joystick, haptic, hidapi, sensor, stay on).
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
if(ANDROID)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(${GT_TP}/SDL ${CMAKE_CURRENT_BINARY_DIR}/SDL)

# ---- Build stamp ---------------------------------------------------------------------------------------------
set(GT_BUILD_NUMBER 0 CACHE STRING "The GitHub release's build number (docs/RELEASE_PIPELINE.md); 0 for a local build")
add_custom_target(gt_stamp
  COMMAND ${CMAKE_COMMAND} -DOUT=${GT_GEN}/build_stamp.h -DVERSION=${PROJECT_VERSION} -DROOT=${GT_ROOT}
          -DBUILD_NUMBER=${GT_BUILD_NUMBER}
          -P ${GT_ROOT}/cmake/BuildStamp.cmake
  BYPRODUCTS ${GT_GEN}/build_stamp.h
  COMMENT "build stamp")

# ---- Shaders: validated as GLSL ES 3.00 with their prelude, embedded as raw strings (TECH_SPEC 2.1, 5.1) -------
file(GLOB GT_SHADER_FILES CONFIGURE_DEPENDS ${GT_ROOT}/shaders/*.vert ${GT_ROOT}/shaders/*.frag)
file(GLOB GT_SHADER_PRELUDES CONFIGURE_DEPENDS ${GT_ROOT}/shaders/*.glsl)
find_program(GT_GLSLANG glslangValidator HINTS $ENV{VULKAN_SDK}/Bin C:/VulkanSDK/1.4.363.0/Bin)
set(GT_SHADER_STAMPS)
if(GT_GLSLANG)
  foreach(src ${GT_SHADER_FILES})
    get_filename_component(name ${src} NAME)
    set(stamp ${GT_GEN}/glsl/${name}.ok)
    add_custom_command(OUTPUT ${stamp}
      COMMAND ${CMAKE_COMMAND} -DIN=${src} -DSHADERS=${GT_ROOT}/shaders -DOUT=${GT_GEN}/glsl/${name}
              -DGLSLANG=${GT_GLSLANG} -DSTAMP=${stamp} -P ${GT_ROOT}/cmake/ValidateShader.cmake
      DEPENDS ${src} ${GT_SHADER_PRELUDES} ${GT_ROOT}/cmake/ValidateShader.cmake
      COMMENT "glslangValidator ${name}"
      VERBATIM)
    list(APPEND GT_SHADER_STAMPS ${stamp})
  endforeach()
else()
  message(WARNING "glslangValidator not found: shaders are not validated at build time")
endif()
add_custom_command(OUTPUT ${GT_GEN}/shader_sources.inc
  COMMAND ${CMAKE_COMMAND} -DSHADERS=${GT_ROOT}/shaders -DOUT=${GT_GEN}/shader_sources.inc -P ${GT_ROOT}/cmake/EmbedShaders.cmake
  DEPENDS ${GT_SHADER_FILES} ${GT_SHADER_PRELUDES} ${GT_ROOT}/cmake/EmbedShaders.cmake ${GT_SHADER_STAMPS}
  COMMENT "embed shaders"
  VERBATIM)
add_custom_target(gt_shaders DEPENDS ${GT_GEN}/shader_sources.inc)

set(GT_WARN -Wall -Wextra -Wno-missing-field-initializers -Werror)
set(GT_LANG_FLAGS $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -fno-math-errno)

# ---- Libraries -----------------------------------------------------------------------------------------------
file(GLOB GT_CORE_SRC CONFIGURE_DEPENDS ${GT_ROOT}/src/core/*.cpp)
if(GT_SANITIZE)
  list(FILTER GT_CORE_SRC EXCLUDE REGEX "alloc_counter\\.cpp$")
endif()
add_library(gt_core STATIC ${GT_CORE_SRC})
target_include_directories(gt_core PUBLIC ${GT_ROOT}/src ${GT_GEN})
target_compile_options(gt_core PUBLIC ${GT_LANG_FLAGS} PRIVATE ${GT_WARN})
add_dependencies(gt_core gt_stamp)
if(ANDROID)
  target_link_libraries(gt_core PUBLIC log)
endif()

file(GLOB GT_SIM_SRC CONFIGURE_DEPENDS ${GT_ROOT}/src/sim/*.cpp)
add_library(gt_sim STATIC ${GT_SIM_SRC})
target_link_libraries(gt_sim PUBLIC gt_core)
# Determinism (TECH_SPEC 4.2): no contraction, no fast-math; optimised even in Debug so bots and soaks are fast.
target_compile_options(gt_sim PRIVATE ${GT_WARN} -ffp-contract=off $<$<CONFIG:Debug>:-O2>)

file(GLOB GT_AUDIO_SRC CONFIGURE_DEPENDS ${GT_ROOT}/src/audio/*.cpp)
add_library(gt_audio STATIC ${GT_AUDIO_SRC})
target_link_libraries(gt_audio PUBLIC gt_core)
target_compile_options(gt_audio PRIVATE ${GT_WARN} $<$<CONFIG:Debug>:-O2>)

file(GLOB GT_APP_SRC CONFIGURE_DEPENDS ${GT_ROOT}/src/gfx/*.cpp ${GT_ROOT}/src/game/*.cpp ${GT_ROOT}/src/platform/*.cpp)

if(ANDROID)
  add_library(main SHARED ${GT_APP_SRC})
  target_link_libraries(main PRIVATE gt_sim gt_audio gt_core SDL3::SDL3-shared GLESv3 EGL android log)
  target_compile_options(main PRIVATE ${GT_WARN})
  target_link_options(main PRIVATE -Wl,--no-undefined -Wl,-z,max-page-size=16384)
  add_dependencies(main gt_shaders)
elseif(EMSCRIPTEN)
  # One thread (a page served over plain HTTP has no SharedArrayBuffer), WebGL 2 for OpenGL ES 3.0, the canvas and the
  # loading line from web/shell.html; index.html, index.js and index.wasm in the build directory.
  add_executable(glowtide ${GT_APP_SRC})
  set_target_properties(glowtide PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_libraries(glowtide PRIVATE gt_sim gt_audio gt_core SDL3::SDL3-static)
  target_compile_options(glowtide PRIVATE ${GT_WARN})
  target_link_options(glowtide PRIVATE -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=2MB -sENVIRONMENT=web
    "SHELL:--shell-file ${GT_ROOT}/web/shell.html" $<$<CONFIG:Release>:-O3>)
  set_property(TARGET glowtide APPEND PROPERTY LINK_DEPENDS ${GT_ROOT}/web/shell.html)
  add_dependencies(glowtide gt_shaders)
else()
  add_executable(glowtide ${GT_APP_SRC})
  target_include_directories(glowtide SYSTEM PRIVATE ${GT_TP}/khronos)
  target_link_libraries(glowtide PRIVATE gt_sim gt_audio gt_core SDL3::SDL3-static)
  target_compile_options(glowtide PRIVATE ${GT_WARN})
  target_link_options(glowtide PRIVATE ${GT_STATIC_LINK})
  add_dependencies(glowtide gt_shaders)

  add_executable(glowtide_headless ${GT_ROOT}/src/tools/headless.cpp ${GT_ROOT}/src/tools/audio_render.cpp
    ${GT_ROOT}/src/game/feedback.cpp)
  target_link_libraries(glowtide_headless PRIVATE gt_sim gt_audio gt_core)
  target_compile_options(glowtide_headless PRIVATE ${GT_WARN})
  target_link_options(glowtide_headless PRIVATE ${GT_STATIC_LINK})

  add_executable(glowtide_tests ${GT_ROOT}/src/tools/tests.cpp ${GT_ROOT}/src/tools/tests_sim.cpp ${GT_ROOT}/src/gfx/palette.cpp
    ${GT_ROOT}/src/gfx/camera.cpp ${GT_ROOT}/src/gfx/text.cpp ${GT_ROOT}/src/game/save.cpp
    ${GT_ROOT}/src/game/feedback.cpp)
  target_link_libraries(glowtide_tests PRIVATE gt_sim gt_audio gt_core)
  target_compile_options(glowtide_tests PRIVATE ${GT_WARN})
  target_link_options(glowtide_tests PRIVATE ${GT_STATIC_LINK})
endif()
