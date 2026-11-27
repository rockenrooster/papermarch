# PAPERMARCH - one CMake tree for the Windows dev build, the Android libmain.so and the browser build (TECH_SPEC 2).
# Desktop: scripts/build-desktop.ps1 (native CMake 4.x, Ninja, llvm-mingw clang).
# Android: android/ Gradle project points externalNativeBuild here (SDK CMake 3.22.1, NDK r29).
# Browser: scripts/build-web.ps1 (Emscripten: WebAssembly and WebGL 2, the page from web/shell.html).

cmake_minimum_required(VERSION 3.22)
project(papermarch VERSION 0.9.0 LANGUAGES C CXX)  # (android/app/build.gradle versionName is kept the same)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(PM_SANITIZE "AddressSanitizer + UBSan build of the tests and the headless tool" OFF)
# Development builds (Debug, RelWithDebInfo) keep the dev-only tools such as the level stepper; Release drops them.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_definitions(PM_DEV=1)
endif()
set(PM_STATIC_LINK -static)
if(PM_SANITIZE)
  add_compile_definitions(PM_SANITIZE=1)
  set(PM_STATIC_LINK "")
endif()

set(PM_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PM_TP ${PM_ROOT}/third_party)
set(PM_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PM_GEN})
if(NOT EXISTS ${PM_TP}/SDL/CMakeLists.txt)
  message(FATAL_ERROR "third_party/SDL is missing - run scripts/fetch-deps.ps1")
endif()

# Owner's rule: at most 4 parallel jobs, also for the Ninja that Gradle starts.
set(PM_MAX_JOBS 4 CACHE STRING "Parallel compile and link jobs (Ninja job pool)")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS pm_jobs=${PM_MAX_JOBS})
set(CMAKE_JOB_POOL_COMPILE pm_jobs)
set(CMAKE_JOB_POOL_LINK pm_jobs)

if(ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-g)  # unstripped libraries are kept beside the release APK
endif()

# ---- SDL3: static on the desktop and in the browser, shared on Android ------------------------------------------
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
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
add_subdirectory(${PM_TP}/SDL ${CMAKE_CURRENT_BINARY_DIR}/SDL)

# ---- Build stamp ---------------------------------------------------------------------------------------------
set(PM_BUILD_NUMBER 0 CACHE STRING "The GitHub release's build number (docs/RELEASE_PIPELINE.md); 0 for a local build")
add_custom_target(pm_stamp
  COMMAND ${CMAKE_COMMAND} -DOUT=${PM_GEN}/build_stamp.h -DVERSION=${PROJECT_VERSION} -DROOT=${PM_ROOT}
          -DBUILD_NUMBER=${PM_BUILD_NUMBER}
          -P ${PM_ROOT}/cmake/BuildStamp.cmake
  BYPRODUCTS ${PM_GEN}/build_stamp.h
  COMMENT "build stamp")

# ---- Shaders: validated as GLSL ES 3.00 with their prelude, embedded as raw strings (TECH_SPEC 5) --------------
file(GLOB PM_SHADER_FILES CONFIGURE_DEPENDS ${PM_ROOT}/shaders/*.vert ${PM_ROOT}/shaders/*.frag)
file(GLOB PM_SHADER_PRELUDES CONFIGURE_DEPENDS ${PM_ROOT}/shaders/*.glsl)
find_program(PM_GLSLANG glslangValidator HINTS $ENV{VULKAN_SDK}/Bin C:/VulkanSDK/1.4.363.0/Bin)
set(PM_SHADER_STAMPS)
if(PM_GLSLANG)
  foreach(src ${PM_SHADER_FILES})
    get_filename_component(name ${src} NAME)
    set(stamp ${PM_GEN}/glsl/${name}.ok)
    add_custom_command(OUTPUT ${stamp}
      COMMAND ${CMAKE_COMMAND} -DIN=${src} -DSHADERS=${PM_ROOT}/shaders -DOUT=${PM_GEN}/glsl/${name}
              -DGLSLANG=${PM_GLSLANG} -DSTAMP=${stamp} -P ${PM_ROOT}/cmake/ValidateShader.cmake
      DEPENDS ${src} ${PM_SHADER_PRELUDES} ${PM_ROOT}/cmake/ValidateShader.cmake
      COMMENT "glslangValidator ${name}"
      VERBATIM)
    list(APPEND PM_SHADER_STAMPS ${stamp})
  endforeach()
else()
  message(WARNING "glslangValidator not found: shaders are not validated at build time")
endif()
add_custom_command(OUTPUT ${PM_GEN}/shader_sources.inc
  COMMAND ${CMAKE_COMMAND} -DSHADERS=${PM_ROOT}/shaders -DOUT=${PM_GEN}/shader_sources.inc -P ${PM_ROOT}/cmake/EmbedShaders.cmake
  DEPENDS ${PM_SHADER_FILES} ${PM_SHADER_PRELUDES} ${PM_ROOT}/cmake/EmbedShaders.cmake ${PM_SHADER_STAMPS}
  COMMENT "embed shaders"
  VERBATIM)
add_custom_target(pm_shaders DEPENDS ${PM_GEN}/shader_sources.inc)

set(PM_WARN -Wall -Wextra -Wno-missing-field-initializers -Werror)
set(PM_LANG_FLAGS $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -fno-math-errno)

# ---- Libraries -----------------------------------------------------------------------------------------------
file(GLOB PM_CORE_SRC CONFIGURE_DEPENDS ${PM_ROOT}/src/core/*.cpp)
if(PM_SANITIZE)
  list(FILTER PM_CORE_SRC EXCLUDE REGEX "alloc_counter\\.cpp$")
endif()
add_library(pm_core STATIC ${PM_CORE_SRC})
target_include_directories(pm_core PUBLIC ${PM_ROOT}/src ${PM_GEN})
target_compile_options(pm_core PUBLIC ${PM_LANG_FLAGS} PRIVATE ${PM_WARN})
add_dependencies(pm_core pm_stamp)
if(ANDROID)
  target_link_libraries(pm_core PUBLIC log)
endif()

# The rules and the solver (TECH_SPEC 3): integers only, optimised even in Debug so the solver and the bake are fast.
file(GLOB PM_SIM_SRC CONFIGURE_DEPENDS ${PM_ROOT}/src/sim/*.cpp)
add_library(pm_sim STATIC ${PM_SIM_SRC})
target_link_libraries(pm_sim PUBLIC pm_core)
target_compile_options(pm_sim PRIVATE ${PM_WARN} $<$<CONFIG:Debug>:-O2>)

# The generator and the bake (TECH_SPEC 4): the headless tool and the tests only, never the app.
file(GLOB PM_GEN_SRC CONFIGURE_DEPENDS ${PM_ROOT}/src/gen/*.cpp)
add_library(pm_gen STATIC ${PM_GEN_SRC})
target_link_libraries(pm_gen PUBLIC pm_sim)
target_compile_options(pm_gen PRIVATE ${PM_WARN} $<$<CONFIG:Debug>:-O2>)

file(GLOB PM_AUDIO_SRC CONFIGURE_DEPENDS ${PM_ROOT}/src/audio/*.cpp)
add_library(pm_audio STATIC ${PM_AUDIO_SRC})
target_link_libraries(pm_audio PUBLIC pm_core)
target_compile_options(pm_audio PRIVATE ${PM_WARN} $<$<CONFIG:Debug>:-O2>)

file(GLOB PM_APP_SRC CONFIGURE_DEPENDS ${PM_ROOT}/src/gfx/*.cpp ${PM_ROOT}/src/game/*.cpp ${PM_ROOT}/src/platform/*.cpp)

if(ANDROID)
  add_library(main SHARED ${PM_APP_SRC})
  target_link_libraries(main PRIVATE pm_sim pm_audio pm_core SDL3::SDL3-shared GLESv3 EGL android log)
  target_compile_options(main PRIVATE ${PM_WARN})
  target_link_options(main PRIVATE -Wl,--no-undefined -Wl,-z,max-page-size=16384)
  add_dependencies(main pm_shaders)
elseif(EMSCRIPTEN)
  # One thread (a page served over plain HTTP has no SharedArrayBuffer), WebGL 2 for OpenGL ES 3.0, the canvas and the
  # loading line from web/shell.html; index.html, index.js and index.wasm in the build directory.
  add_executable(papermarch ${PM_APP_SRC})
  set_target_properties(papermarch PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_libraries(papermarch PRIVATE pm_sim pm_audio pm_core SDL3::SDL3-static)
  target_compile_options(papermarch PRIVATE ${PM_WARN})
  target_link_options(papermarch PRIVATE -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=96MB -sSTACK_SIZE=2MB -sENVIRONMENT=web
    "SHELL:--shell-file ${PM_ROOT}/web/shell.html" $<$<CONFIG:Release>:-O3>)
  set_property(TARGET papermarch APPEND PROPERTY LINK_DEPENDS ${PM_ROOT}/web/shell.html)
  # The art (assets/art) as preloaded files under /art: index.data beside index.wasm.
  if(EXISTS ${PM_ROOT}/assets/art/sprites.txt)
    target_link_options(papermarch PRIVATE "SHELL:--preload-file ${PM_ROOT}/assets/art@/art")
    file(GLOB PM_ART_FILES CONFIGURE_DEPENDS ${PM_ROOT}/assets/art/*)
    set_property(TARGET papermarch APPEND PROPERTY LINK_DEPENDS ${PM_ART_FILES})
  endif()
  add_dependencies(papermarch pm_shaders)
else()
  add_executable(papermarch ${PM_APP_SRC})
  target_include_directories(papermarch SYSTEM PRIVATE ${PM_TP}/khronos)
  target_link_libraries(papermarch PRIVATE pm_sim pm_audio pm_core SDL3::SDL3-static)
  target_compile_options(papermarch PRIVATE ${PM_WARN})
  target_compile_definitions(papermarch PRIVATE PM_ART_DIR="${PM_ROOT}/assets/art")  # the art, read in place
  target_link_options(papermarch PRIVATE ${PM_STATIC_LINK})
  add_dependencies(papermarch pm_shaders)

  add_executable(papermarch_headless ${PM_ROOT}/src/tools/headless.cpp ${PM_ROOT}/src/tools/audio_render.cpp)
  target_link_libraries(papermarch_headless PRIVATE pm_gen pm_sim pm_audio pm_core)
  target_compile_options(papermarch_headless PRIVATE ${PM_WARN})
  target_link_options(papermarch_headless PRIVATE ${PM_STATIC_LINK})

  add_executable(papermarch_tests ${PM_ROOT}/src/tools/tests.cpp ${PM_ROOT}/src/gfx/palette.cpp ${PM_ROOT}/src/gfx/text.cpp
    ${PM_ROOT}/src/game/save.cpp ${PM_ROOT}/src/game/layout.cpp)
  target_link_libraries(papermarch_tests PRIVATE pm_gen pm_sim pm_audio pm_core)
  target_compile_options(papermarch_tests PRIVATE ${PM_WARN})
  target_link_options(papermarch_tests PRIVATE ${PM_STATIC_LINK})
endif()
