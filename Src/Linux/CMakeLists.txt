# Winamp for Linux: native GTK 3 player with the classic skin engine,
# the original Winamp plug-in architecture (in_*.so / out_*.so) and the
# portable parts of the Windows code base (tagz, EQ10, classic vis).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)


set(WA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/..)       # Src/
set(WA_OUT ${CMAKE_BINARY_DIR})                  # winamp binary + Plugins/ next to it
set(WA_PLUGIN_OUT ${WA_OUT}/Plugins)
set(WA_PLUGIN_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/winamp/Plugins)

# --- the title formatting engine from Src/tagz, compiled unmodified ------------
add_library(wa_tagz STATIC
	${WA_SRC}/tagz/FMT.cpp
	${WA_SRC}/tagz/functions.cpp
	${WA_SRC}/tagz/string.cpp
	${WA_SRC}/tagz/varlist.cpp)
target_include_directories(wa_tagz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compat/include)
target_compile_options(wa_tagz PRIVATE -include tagz_linux_prelude.h -w)

# --- player core (no GUI) ------------------------------------------------------
add_library(wa_core STATIC
	core/common.cpp
	core/config.cpp
	core/ini.cpp
	core/eq.cpp
	core/eq10dsp.cpp
	core/nsutil_fft.cpp
	core/player.cpp
	core/playlist.cpp
	core/plugins.cpp
	core/titleformat.cpp
	core/vis.cpp)
target_include_directories(wa_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# draw_sa.cpp keeps the original one-line "if (v < 0) v = 0; if (v > 15) v = 15;" style
set_source_files_properties(ui/draw_sa.cpp PROPERTIES COMPILE_OPTIONS "-Wno-misleading-indentation")
set_source_files_properties(core/titleformat.cpp PROPERTIES COMPILE_OPTIONS "-include;tagz_linux_prelude.h")
target_compile_definitions(wa_core PRIVATE WINAMP_LIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}/winamp")
target_link_libraries(wa_core PUBLIC wa_tagz PkgConfig::GLIB Threads::Threads ${CMAKE_DL_LIBS})

option(WINAMP_BUILD_GUI "Build the player (off: only the core library and plug-ins)" ON)

if(WINAMP_BUILD_GUI)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)

# --- built-in "Winamp Classic" skin: the original bitmaps from Src/Winamp/resource
set(WA_SKIN_DIR ${WA_SRC}/Winamp/resource)
set(WA_GRESOURCE_XML ${CMAKE_CURRENT_BINARY_DIR}/winamp.gresource.xml)
set(WA_GRESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/winamp_resources.c)
set(WA_SKIN_FILES
	"main.bmp=MAIN.BMP" "cbuttons.bmp=CBUTTONS.BMP" "monoster.bmp=MONOSTER.BMP"
	"playpaus.bmp=PLAYPAUS.BMP" "shufrep.bmp=SHUFREP.BMP" "numbers.bmp=numbers.bmp"
	"volume.bmp=volume.bmp" "balance.bmp=BALANCE.BMP" "text.bmp=text.bmp"
	"posbar.bmp=POSBAR.BMP" "titlebar.bmp=titlebar.bmp" "eqmain.bmp=Eqmain.bmp"
	"eqmain_iso.bmp=Eqmain_ISO.bmp" "eq_ex.bmp=eq_ex.bmp" "pledit.bmp=Pledit.bmp"
	"gen.bmp=gen.bmp" "winamp.ico=WinampIcon.ico")
set(WA_GRESOURCE_DEPS)
set(WA_GRESOURCE_BODY "")
foreach(entry ${WA_SKIN_FILES})
	string(REPLACE "=" ";" pair ${entry})
	list(GET pair 0 alias)
	list(GET pair 1 file)
	string(APPEND WA_GRESOURCE_BODY "    <file alias=\"${alias}\">${file}</file>\n")
	list(APPEND WA_GRESOURCE_DEPS ${WA_SKIN_DIR}/${file})
endforeach()
file(WRITE ${WA_GRESOURCE_XML}
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gresources>\n  <gresource prefix=\"/winamp/skin\">\n${WA_GRESOURCE_BODY}  </gresource>\n</gresources>\n")
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
add_custom_command(OUTPUT ${WA_GRESOURCE_C}
	COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${WA_GRESOURCE_C}
		--sourcedir=${WA_SKIN_DIR} ${WA_GRESOURCE_XML}
	DEPENDS ${WA_GRESOURCE_XML} ${WA_GRESOURCE_DEPS}
	COMMENT "Embedding the classic skin bitmaps")

# --- the player ------------------------------------------------------------------
add_executable(winamp
	main.cpp
	ui/bmp.cpp
	ui/commands.cpp
	ui/dialogs.cpp
	ui/docking.cpp
	ui/draw_eq.cpp
	ui/draw_main.cpp
	ui/draw_pe.cpp
	ui/draw_sa.cpp
	ui/gfx.cpp
	ui/keyboard.cpp
	ui/menus.cpp
	ui/mpris.cpp
	ui/skin.cpp
	ui/skinwnd.cpp
	ui/ui_eq.cpp
	ui/ui_main.cpp
	ui/ui_pe.cpp
	${WA_GRESOURCE_C})
target_link_libraries(winamp PRIVATE wa_core PkgConfig::GTK3 PkgConfig::LIBZIP)
set_target_properties(winamp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${WA_OUT})
install(TARGETS winamp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/winamp.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/winamp.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()

# --- plug-ins --------------------------------------------------------------------
function(wa_plugin name)
	cmake_parse_arguments(P "" "" "SOURCES;PKG" ${ARGN})
	add_library(${name} MODULE ${P_SOURCES})
	set_target_properties(${name} PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${WA_PLUGIN_OUT})
	target_link_libraries(${name} PRIVATE Threads::Threads ${P_PKG})
	target_link_options(${name} PRIVATE -Wl,--no-undefined)
	install(TARGETS ${name} LIBRARY DESTINATION ${WA_PLUGIN_INSTALL_DIR})
	list(APPEND WA_BUILT_PLUGINS ${name})
	set(WA_BUILT_PLUGINS ${WA_BUILT_PLUGINS} PARENT_SCOPE)
endfunction()

set(WA_BUILT_PLUGINS)
set(WA_MISSING_PLUGINS)

pkg_check_modules(MPG123 IMPORTED_TARGET libmpg123)
if(MPG123_FOUND)
	wa_plugin(in_mp3 SOURCES plugins/in_mp3/in_mp3.cpp PKG PkgConfig::MPG123)
else()
	list(APPEND WA_MISSING_PLUGINS "in_mp3 (libmpg123)")
endif()

pkg_check_modules(VORBISFILE IMPORTED_TARGET vorbisfile)
if(VORBISFILE_FOUND)
	wa_plugin(in_vorbis SOURCES plugins/in_vorbis/in_vorbis.cpp PKG PkgConfig::VORBISFILE)
else()
	list(APPEND WA_MISSING_PLUGINS "in_vorbis (vorbisfile)")
endif()

pkg_check_modules(FLAC IMPORTED_TARGET flac)
if(FLAC_FOUND)
	wa_plugin(in_flac SOURCES plugins/in_flac/in_flac.cpp PKG PkgConfig::FLAC)
else()
	list(APPEND WA_MISSING_PLUGINS "in_flac (flac)")
endif()

pkg_check_modules(SNDFILE IMPORTED_TARGET sndfile)
if(SNDFILE_FOUND)
	wa_plugin(in_wave SOURCES plugins/in_wave/in_wave.cpp PKG PkgConfig::SNDFILE)
else()
	list(APPEND WA_MISSING_PLUGINS "in_wave (sndfile)")
endif()

pkg_check_modules(OPENMPT IMPORTED_TARGET libopenmpt)
if(OPENMPT_FOUND)
	wa_plugin(in_mod SOURCES plugins/in_mod/in_mod.cpp PKG PkgConfig::OPENMPT)
else()
	list(APPEND WA_MISSING_PLUGINS "in_mod (libopenmpt)")
endif()

pkg_check_modules(PULSE IMPORTED_TARGET libpulse)
if(PULSE_FOUND)
	wa_plugin(out_pulse SOURCES plugins/out_pulse/out_pulse.cpp PKG PkgConfig::PULSE)
else()
	list(APPEND WA_MISSING_PLUGINS "out_pulse (libpulse)")
endif()

pkg_check_modules(ALSA IMPORTED_TARGET alsa)
if(ALSA_FOUND)
	wa_plugin(out_alsa SOURCES plugins/out_alsa/out_alsa.cpp PKG PkgConfig::ALSA)
else()
	list(APPEND WA_MISSING_PLUGINS "out_alsa (alsa)")
endif()

wa_plugin(out_disk SOURCES plugins/out_disk/out_disk.cpp)

# the player finds its plug-ins in ./Plugins when run from the build tree
if(WINAMP_BUILD_GUI)
	add_dependencies(winamp ${WA_BUILT_PLUGINS})
endif()

message(STATUS "Winamp plug-ins: ${WA_BUILT_PLUGINS}")
if(WA_MISSING_PLUGINS)
	message(STATUS "Winamp plug-ins skipped (missing libraries): ${WA_MISSING_PLUGINS}")
endif()

# --- tests -------------------------------------------------------------------------
option(WINAMP_BUILD_TESTS "Build the core self tests" ON)
if(WINAMP_BUILD_TESTS)
	enable_testing()
	add_executable(wa_core_test tests/core_test.cpp)
	target_link_libraries(wa_core_test PRIVATE wa_core)
	add_dependencies(wa_core_test ${WA_BUILT_PLUGINS})
	add_test(NAME wa_core_test COMMAND wa_core_test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	set_tests_properties(wa_core_test PROPERTIES ENVIRONMENT "WINAMP_PLUGIN_DIR=${WA_PLUGIN_OUT}")
endif()
