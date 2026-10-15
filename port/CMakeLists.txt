cmake_minimum_required(VERSION 3.13)
project(bonanza_port C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BONANZA_HEADLESS "Build the headless test runner" ON)
option(BONANZA_FETCH_SDL "Download SDL2 and link it statically (self-contained binary)" OFF)
set(BONANZA_SDL_URL "https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz"
    CACHE STRING "SDL2 source archive used with BONANZA_FETCH_SDL")

set(CORE_SOURCES
  src/md.c src/vdp.c src/psg.c src/sha1.c src/cpu68k.c src/settings.c src/menu.c src/wideobj.c src/gamemods.c src/extrastages.c src/cheats.c src/net.c src/mods.c
  src/m68k_rt.c src/sndbank.c src/sndrec.c src/sndmix.c src/crt.c
  ext/nuked-opn2/ym3438.c
)

list(APPEND CORE_SOURCES src/slotcode.c src/draw.c src/game.c src/engine/recomp_cycles.c src/engine/recomp_slot.c)

# The game's code, translated to C from the ROM (tools/recomp.py, tools/z80recomp.py;
# not in the repository). With the ROM it is made here and linked into the program;
# with BONANZA_GAME_LIB the program is the engine alone and loads the game's code as a
# library (bonanza_game.dll / libbonanza_game.so: the builder pages make it from the
# player's ROM in the browser; with the ROM here it is built too).
set(BONANZA_ROM "${CMAKE_CURRENT_SOURCE_DIR}/../baserom.md" CACHE FILEPATH "the ROM (Bonanza Bros., JUE)")
option(BONANZA_GAME_LIB "The engine alone: the game's code is a library made from the ROM" OFF)
set(GAME_GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(GAME_SOURCES "")
if(EXISTS ${BONANZA_ROM})
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(NOT EXISTS ${GAME_GEN}/game_link.c OR ${BONANZA_ROM} IS_NEWER_THAN ${GAME_GEN}/game_link.c)
    message(STATUS "Translating the game's code from ${BONANZA_ROM}")
    execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/recomp.py ${BONANZA_ROM} --out ${GAME_GEN}
                    RESULT_VARIABLE r1 OUTPUT_QUIET)
    execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/z80recomp.py ${BONANZA_ROM} --out ${GAME_GEN}
                    RESULT_VARIABLE r2 OUTPUT_QUIET)
    if(r1 OR r2)
      file(REMOVE ${GAME_GEN}/game_link.c)
      message(FATAL_ERROR "Translating the game's code failed (is the ROM the right one?)")
    endif()
  endif()
  file(GLOB GAME_SOURCES ${GAME_GEN}/recomp_0*.c)
  list(APPEND GAME_SOURCES ${GAME_GEN}/recomp_index.c ${GAME_GEN}/m68k_costs.c ${GAME_GEN}/z80_driver.c)
elseif(NOT BONANZA_GAME_LIB)
  message(FATAL_ERROR "No ROM (${BONANZA_ROM}): give BONANZA_ROM, or build the engine alone with -DBONANZA_GAME_LIB=ON")
endif()
if(NOT BONANZA_GAME_LIB)
  list(APPEND CORE_SOURCES ${GAME_SOURCES})
else()
  list(APPEND CORE_SOURCES src/engine/game_syms.c)
endif()

add_library(bonanza_core STATIC ${CORE_SOURCES})
target_include_directories(bonanza_core PUBLIC src ext/musashi)
if(NOT BONANZA_GAME_LIB)
  target_compile_definitions(bonanza_core PRIVATE BONANZA_GAME_STATIC)
elseif(UNIX)
  target_link_libraries(bonanza_core PUBLIC ${CMAKE_DL_LIBS})
endif()

# the game's library built here (with the ROM and BONANZA_GAME_LIB): as the builder pages
# build it, with nothing from the system
if(BONANZA_GAME_LIB AND GAME_SOURCES AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include OUTPUT_VARIABLE CC_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)
  add_library(bonanza_game SHARED ${GAME_SOURCES} ${GAME_GEN}/game_link.c)
  set_target_properties(bonanza_game PROPERTIES PREFIX "lib" OUTPUT_NAME bonanza_game C_VISIBILITY_PRESET hidden)
  if(WIN32)
    set_target_properties(bonanza_game PROPERTIES PREFIX "")
  endif()
  target_include_directories(bonanza_game PRIVATE src ext/musashi)
  target_compile_options(bonanza_game PRIVATE -O1 -w -fcommon -nostdinc
                         "SHELL:-isystem ${CMAKE_CURRENT_SOURCE_DIR}/gamelib/include" "SHELL:-isystem ${CC_INCLUDE}"
                         "SHELL:-include ${GAME_GEN}/game_bind.h")
  target_link_options(bonanza_game PRIVATE -nostdlib)
endif()

# Opus: the game's recorded sounds are kept in it (sndbank.c)
set(BONANZA_OPUS_URL "https://github.com/xiph/opus/releases/download/v1.5.2/opus-1.5.2.tar.gz"
    CACHE STRING "libopus source archive")
include(FetchContent)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
  set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(opus URL ${BONANZA_OPUS_URL})
FetchContent_MakeAvailable(opus)
target_link_libraries(bonanza_core PUBLIC opus)
if(MSVC)
  target_compile_options(bonanza_core PRIVATE /W0)
else()
  target_compile_options(bonanza_core PRIVATE -w)
  if(GAME_SOURCES AND NOT BONANZA_GAME_LIB)
    set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-w")
  endif()
  # the CRT filter: its row loops are vectorised (no errno or traps from its maths)
  set_source_files_properties(src/crt.c PROPERTIES COMPILE_OPTIONS "-O3;-fno-math-errno;-fno-trapping-math")
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
  target_link_libraries(bonanza_core PUBLIC m)
endif()
if(WIN32)
  target_link_libraries(bonanza_core PUBLIC ws2_32)   # online play (net.c)
endif()

if(EMSCRIPTEN AND BONANZA_GAME_LIB)
  # the builder pages' sound recorder (src/sndrec_web.c): the game's sounds recorded from
  # the player's ROM while the page builds the package; one file, run in page workers
  add_executable(bonanza-sounds src/sndrec_web.c ext/z80/z80.c)
  target_link_libraries(bonanza-sounds bonanza_core)
  target_compile_options(bonanza-sounds PRIVATE -O3)
  set_target_properties(bonanza-sounds PROPERTIES SUFFIX ".js" LINK_FLAGS
    "-O3 -sMODULARIZE=1 -sEXPORT_NAME=BonanzaSounds -sSINGLE_FILE=1 -sENVIRONMENT=worker,node -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2147483648 -sEXPORTED_FUNCTIONS=['_sndweb_ids','_sndweb_rom','_sndweb_record','_malloc','_free'] -sEXPORTED_RUNTIME_METHODS=['FS','HEAPU8'] -sINVOKE_RUN=0")
  return()
endif()

if(EMSCRIPTEN)
  add_executable(bonanza src/main_sdl.c)
  set(EMFLAGS "-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sEXPORTED_RUNTIME_METHODS=['FS','callMain','ccall'] -sINVOKE_RUN=0 -sENVIRONMENT=web -sSTACK_SIZE=1048576")
  if(BONANZA_FETCH_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2 URL ${BONANZA_SDL_URL})
    FetchContent_MakeAvailable(SDL2)
    target_link_libraries(bonanza bonanza_core SDL2::SDL2main SDL2::SDL2-static)
  else()
    target_compile_options(bonanza PRIVATE -sUSE_SDL=2)
    set(EMFLAGS "${EMFLAGS} -sUSE_SDL=2")
    target_link_libraries(bonanza bonanza_core)
  endif()
  set_target_properties(bonanza PROPERTIES LINK_FLAGS "${EMFLAGS}" SUFFIX ".js")
  # The same runtime in one file (wasm inside the .js): the level maker's test play uses
  # it, also when its page is opened from disk, where browsers do not load .wasm files.
  add_executable(bonanza-single src/main_sdl.c)
  get_target_property(BONANZA_LIBS bonanza LINK_LIBRARIES)
  get_target_property(BONANZA_COPTS bonanza COMPILE_OPTIONS)
  target_link_libraries(bonanza-single ${BONANZA_LIBS})
  if(BONANZA_COPTS)
    target_compile_options(bonanza-single PRIVATE ${BONANZA_COPTS})
  endif()
  set_target_properties(bonanza-single PROPERTIES LINK_FLAGS "${EMFLAGS} -sSINGLE_FILE=1" SUFFIX ".js")
  # the game's sounds made by a native build (see the end of this file): the page
  # fetches sounds.bbs from next to the game
  set(BONANZA_SOUNDS "" CACHE FILEPATH "sounds.bbs made already (used as it is)")
  if(BONANZA_SOUNDS)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sounds.bbs
      COMMAND ${CMAKE_COMMAND} -E copy ${BONANZA_SOUNDS} ${CMAKE_CURRENT_BINARY_DIR}/sounds.bbs
      DEPENDS ${BONANZA_SOUNDS}
      COMMENT "Copying the game's sounds")
    add_custom_target(sounds ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/sounds.bbs)
  endif()
  return()
endif()

if(ANDROID)
  # Built from android/ by Gradle. SDL2 is built as libSDL2.so, which SDLActivity loads
  # together with our libmain.so.
  include(FetchContent)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2 URL ${BONANZA_SDL_URL})
  FetchContent_MakeAvailable(SDL2)
  add_library(main SHARED src/main_sdl.c)
  if(TARGET SDL2::SDL2main)
    target_link_libraries(main SDL2::SDL2main)
  endif()
  target_link_libraries(main bonanza_core SDL2::SDL2)
  return()
endif()

if(BONANZA_FETCH_SDL)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2 URL ${BONANZA_SDL_URL})
  FetchContent_MakeAvailable(SDL2)
  add_executable(bonanza WIN32 src/main_sdl.c)
  target_link_libraries(bonanza bonanza_core SDL2::SDL2main SDL2::SDL2-static)
  if(MINGW)
    target_link_options(bonanza PRIVATE -static -static-libgcc)
  endif()
  set(SDL2_FOUND TRUE)
  set(BONANZA_SDL_DONE TRUE)
endif()

if(NOT BONANZA_SDL_DONE)
find_package(SDL2 QUIET CONFIG)
endif()
if(NOT SDL2_FOUND)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 sdl2)
  endif()
endif()

if(BONANZA_SDL_DONE)
  # already configured above
elseif(TARGET SDL2::SDL2)
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

# The game's sounds, recorded from the ROM by the build (sounds.bbs next to the game:
# lossless, 104 MB; with BONANZA_SOUNDS_OPUS in Opus, 22 MB), so the game does not record
# them on its first start. The file depends only on the ROM:
# a cross build (Windows, web) can take the one a native build made (BONANZA_SOUNDS).
# Without either the game records them itself the first time.
set(BONANZA_SOUNDS "" CACHE FILEPATH "sounds.bbs made already (used as it is)")
option(BONANZA_SOUNDS_OPUS "Record the sounds in Opus (about 22 MB) rather than lossless (104 MB)" OFF)
set(SOUNDS_OUT "${CMAKE_CURRENT_BINARY_DIR}/sounds.bbs")
if(BONANZA_SOUNDS)
  add_custom_command(OUTPUT ${SOUNDS_OUT}
    COMMAND ${CMAKE_COMMAND} -E copy ${BONANZA_SOUNDS} ${SOUNDS_OUT}
    DEPENDS ${BONANZA_SOUNDS}
    COMMENT "Copying the game's sounds")
  add_custom_target(sounds ALL DEPENDS ${SOUNDS_OUT})
elseif(BONANZA_HEADLESS AND NOT CMAKE_CROSSCOMPILING AND EXISTS ${BONANZA_ROM} AND NOT BONANZA_GAME_LIB)
  add_custom_command(OUTPUT ${SOUNDS_OUT}
    COMMAND bonanza-headless ${BONANZA_ROM} --make-sounds ${SOUNDS_OUT} $<$<BOOL:${BONANZA_SOUNDS_OPUS}>:--opus>
    DEPENDS ${BONANZA_ROM} src/sndrec.c src/sndbank.c ext/nuked-opn2/ym3438.c src/psg.c ${GAME_GEN}/z80_driver.c
    COMMENT "Recording the game's sounds from the ROM (a minute or two)")
  add_custom_target(sounds ALL DEPENDS ${SOUNDS_OUT})
  add_dependencies(sounds bonanza-headless)
endif()
