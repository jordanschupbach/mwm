cmake_minimum_required(VERSION 3.10)

project(
  mwm
  VERSION 0.1
  DESCRIPTION "My window manager (dwm clone)"
  LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(PkgConfig REQUIRED)

pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
pkg_check_modules(LUA REQUIRED lua)
pkg_check_modules(XFT REQUIRED xft)
pkg_check_modules(XINERAMA REQUIRED xinerama)
pkg_check_modules(GLU REQUIRED glu)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(DBUS REQUIRED dbus-1)
# find_package(X11 REQUIRED)
# find_package(OpenGL REQUIRED)
# find_package(GLU REQUIRED)
include_directories(
  ${HARFBUZZ_INCLUDE_DIRS}
  ${FREETYPE_INCLUDE_DIRS}
  ${FONTCONFIG_INCLUDE_DIRS}
  ${LUA_INCLUDE_DIRS}
  ${XFT_INCLUDE_DIRS}
  ${XINERAMA_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${GLU_INCLUDE_DIR}
  ${DBUS_INCLUDE_DIRS}
)
link_directories(
  ${HARFBUZZ_LIBRARY_DIRS}
  ${FREETYPE_LIBRARY_DIRS}
  ${FONTCONFIG_LIBRARY_DIRS}
  ${LUA_LIBRARY_DIRS}
  ${X11_LIBRARY_DIRS}
  ${XFT_LIBRARY_DIRS}
  ${XINERAMA_LIBRARY_DIRS}
  ${DBUS_LIBRARY_DIRS}
)

add_executable(mwm mwm.cpp)
add_executable(mwm-cli mwm-cli.c)

target_link_libraries(mwm
  ${HARFBUZZ_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${FONTCONFIG_LIBRARIES}
  ${LUA_LIBRARIES}
  ${XFT_LIBRARIES}
  ${XINERAMA_LIBRARIES}
  ${DBUS_LIBRARIES}
  X11
  GL
  GLU
  m
)

install(TARGETS mwm mwm-cli DESTINATION bin)

