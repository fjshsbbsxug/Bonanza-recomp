cmake_minimum_required(VERSION 3.13)
project(bonanza_port C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BONANZA_RECOMP   "Link the statically recompiled 68000 code (gen/)" ON)
option(BONANZA_HEADLESS "Build the headless test runner" ON)

set(CORE_SOURCES
  src/md.c src/vdp.c src/psg.c src/sha1.c src/cpu68k.c
  ext/musashi/m68kcpu.c ext/musashi/m68kops.c ext/musashi/m68kdasm.c
  ext/musashi/softfloat/softfloat.c
  ext/z80/z80.c
  ext/nuked-opn2/ym3438.c
)

file(GLOB RECOMP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/gen/*.c)
if(BONANZA_RECOMP AND RECOMP_SOURCES)
  list(APPEND CORE_SOURCES ${RECOMP_SOURCES})
  message(STATUS "Using recompiled code (${CMAKE_CURRENT_SOURCE_DIR}/gen)")
else()
  list(APPEND CORE_SOURCES src/recomp_stub.c)
  message(STATUS "Recompiled code disabled: interpreter only")
endif()

add_library(bonanza_core STATIC ${CORE_SOURCES})
target_include_directories(bonanza_core PUBLIC src ext/musashi)
if(MSVC)
  target_compile_options(bonanza_core PRIVATE /W0)
else()
  target_compile_options(bonanza_core PRIVATE -w)
  set_source_files_properties(${RECOMP_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-w")
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
  target_link_libraries(bonanza_core PUBLIC m)
endif()

if(EMSCRIPTEN)
  add_executable(bonanza src/main_sdl.c)
  target_link_libraries(bonanza bonanza_core)
  set(EMFLAGS "-sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sEXPORTED_RUNTIME_METHODS=['FS','callMain'] -sINVOKE_RUN=0 -sMODULARIZE=0 -sENVIRONMENT=web")
  target_compile_options(bonanza PRIVATE -sUSE_SDL=2)
  target_compile_options(bonanza_core PRIVATE -sUSE_SDL=2)
  set_target_properties(bonanza PROPERTIES LINK_FLAGS "${EMFLAGS}" SUFFIX ".js")
  return()
endif()

if(ANDROID)
  # Built by the SDL Android project (android/), which provides the SDL2 target.
  add_library(main SHARED src/main_sdl.c)
  target_link_libraries(main bonanza_core SDL2 SDL2main)
  return()
endif()

find_package(SDL2 QUIET CONFIG)
if(NOT SDL2_FOUND)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 sdl2)
  endif()
endif()

if(TARGET SDL2::SDL2)
  add_executable(bonanza WIN32 src/main_sdl.c)
  if(TARGET SDL2::SDL2main)
    target_link_libraries(bonanza bonanza_core SDL2::SDL2main SDL2::SDL2)
  else()
    target_link_libraries(bonanza bonanza_core SDL2::SDL2)
  endif()
elseif(SDL2_FOUND)
  add_executable(bonanza src/main_sdl.c)
  target_include_directories(bonanza PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(bonanza bonanza_core ${SDL2_LIBRARIES})
else()
  message(WARNING "SDL2 not found: only the headless runner will be built")
endif()

if(BONANZA_HEADLESS)
  add_executable(bonanza-headless src/main_headless.c)
  target_link_libraries(bonanza-headless bonanza_core)
endif()
