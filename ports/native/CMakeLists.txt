# High Fidelity Dreams, native port (C + SDL3) for Windows, Linux and macOS.
cmake_minimum_required(VERSION 3.21)
project(hfd VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
# Windows: the C runtime goes into the executable, so it needs no DLL.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# SDL3: the system's if there is one, otherwise built from source and linked statically
# (HFD_VENDOR_SDL forces the latter, as the release builds do).
option(HFD_VENDOR_SDL "Build SDL3 from source and link it statically" OFF)
if(NOT HFD_VENDOR_SDL)
  find_package(SDL3 CONFIG QUIET)
endif()
if(NOT SDL3_FOUND)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
    URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

# The demo's data, read from rip/ at build time (see tools/embed.py).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RIP ${CMAKE_CURRENT_SOURCE_DIR}/../../rip)
file(GLOB RIP_DATA
  ${RIP}/graphics/menu/*.png ${RIP}/graphics/menu/words/*.png
  ${RIP}/graphics/pictures/*.png ${RIP}/graphics/pictures/*.json
  ${RIP}/graphics/pictures/fullscreen/*.png ${RIP}/graphics/pictures/fullscreen/*.json
  ${RIP}/music/*.mod ${RIP}/extracted/textpage/*.bin)
set(ASSETS_C ${CMAKE_CURRENT_BINARY_DIR}/gen/assets.c)
add_custom_command(
  OUTPUT ${ASSETS_C}
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.py ${RIP} ${ASSETS_C}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.py ${RIP_DATA}
  COMMENT "Embedding the demo's data from rip/")

# The icon, drawn from the loading screen's ear (see tools/icon.py).
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
add_custom_command(
  OUTPUT ${GEN}/icon.c ${GEN}/hfd.ico ${GEN}/hfd.rc ${GEN}/hfd.iconset/icon_512x512@2x.png ${GEN}/icon.png
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/icon.py ${RIP} ${GEN}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/icon.py ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.py
    ${RIP}/graphics/pictures/loading_screen_indexed.png ${RIP}/graphics/pictures/loading_screen.json
  COMMENT "Drawing the icon")
set(ICON_SOURCES ${GEN}/icon.c)
if(WIN32)
  # Windows: in the executable's resources, where SDL also finds it for the window.
  list(APPEND ICON_SOURCES ${GEN}/hfd.rc)
  set_source_files_properties(${GEN}/hfd.rc PROPERTIES OBJECT_DEPENDS ${GEN}/hfd.ico)
elseif(APPLE)
  # macOS: an .icns made by the system's own tool, for the Finder, the Dock and Cmd-Tab.
  find_program(ICONUTIL iconutil REQUIRED)
  add_custom_command(
    OUTPUT ${GEN}/hfd.icns
    COMMAND ${ICONUTIL} -c icns ${GEN}/hfd.iconset -o ${GEN}/hfd.icns
    DEPENDS ${GEN}/hfd.iconset/icon_512x512@2x.png
    COMMENT "Making the .icns")
  # And icon.png, which the program gives the Dock and Cmd-Tab when it starts (see main.c).
  list(APPEND ICON_SOURCES ${GEN}/hfd.icns ${GEN}/icon.png)
  set_source_files_properties(${GEN}/hfd.icns ${GEN}/icon.png PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

add_executable(hfd WIN32 MACOSX_BUNDLE
  src/main.c src/display.c src/mod.c src/music.c src/pacing.c src/scene.c
  src/intro.c src/menu.c src/loading.c src/textpage.c ${ASSETS_C} ${ICON_SOURCES})
target_include_directories(hfd PRIVATE src)
target_link_libraries(hfd PRIVATE SDL3::SDL3)
if(NOT MSVC)
  target_compile_options(hfd PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_libraries(hfd PRIVATE m)
endif()

if(APPLE)
  set_target_properties(hfd PROPERTIES
    OUTPUT_NAME "High Fidelity Dreams"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/Info.plist.in
    MACOSX_BUNDLE_ICON_FILE hfd.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()
