cmake_minimum_required(VERSION 3.16)
project(qtdmm VERSION "0.0.0" HOMEPAGE_URL "https://qtdmm.de" LANGUAGES C CXX)

## main settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(PROJECT_LICENSE "GPL-3.0-or-later")
set(RESOURCES "${CMAKE_BINARY_DIR}/resources.qrc")
set(APP_NAME "QtDMM")
set(APP_ORGANIZATION "QtDMM")
set(QT_COMPONENTS Widgets SerialPort PrintSupport Test LinguistTools Network Charts Svg)

include( cmake/git_version.cmake )

## get files
file(GLOB TRANSLATIONS CONFIGURE_DEPENDS assets/translations/${PROJECT_NAME}_*.ts)
file(GLOB DECODER_HEADER_FILES CONFIGURE_DEPENDS src/device/decoders/*.h )
file(GLOB UI_FILES CONFIGURE_DEPENDS src/ui/forms/*.ui)
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp src/*.h )
file(GLOB ICON_FILES CONFIGURE_DEPENDS assets/icons/app/*.png )


## generate resources.qrc so you can't forget to add the icons and translations.
## It lives in the build directory with absolute paths: the source tree stays
## untouched, and several build directories don't overwrite each other's file.
list(SORT ICON_FILES)
set(RES_CONTENT "    <qresource prefix=\"/Symbols\">\n")
foreach(file_path IN LISTS ICON_FILES)
	get_filename_component(base_name "${file_path}" NAME)
	set(RES_CONTENT "${RES_CONTENT}        <file alias=\"${base_name}\">${file_path}</file>\n")
endforeach()
set(RES_CONTENT "${RES_CONTENT}    </qresource>\n")

## Translations are compiled into the binary by default: one file, nothing to
## find at run time. Linux/BSD distributions that package them as files
## (Fedora's %find_lang --with-qt) turn this off; the .qm files are then
## installed to share/qtdmm/translations. Windows and macOS always embed them.
option(QTDMM_EMBED_TRANSLATIONS "Compile the translations into the binary instead of installing them as .qm files" ON)
if (WIN32 OR APPLE)
	set(QTDMM_EMBED_TRANSLATIONS ON)
endif()
if (QTDMM_EMBED_TRANSLATIONS)
	set(RES_CONTENT "${RES_CONTENT}    <qresource prefix=\"/Translations\">\n")
	foreach(translation_file IN LISTS TRANSLATIONS)
		get_filename_component(qm_name "${translation_file}" NAME_WE)
		set(RES_CONTENT "${RES_CONTENT}        <file alias=\"${qm_name}.qm\">${CMAKE_BINARY_DIR}/${qm_name}.qm</file>\n")
	endforeach()
	set(RES_CONTENT "${RES_CONTENT}    </qresource>\n")
endif()

## icon sets (Breeze light and dark, Oxygen; tools/update_icons.py), each an
## icon theme as :/icons/<theme>/... for QIcon::fromTheme()
file(GLOB_RECURSE THEME_FILES CONFIGURE_DEPENDS assets/icons/sets/*/index.theme assets/icons/sets/*.svg
     assets/icons/sets/*.png)
list(SORT THEME_FILES)
set(RES_CONTENT "${RES_CONTENT}    <qresource prefix=\"/icons\">\n")
foreach(file_path IN LISTS THEME_FILES)
	file(RELATIVE_PATH alias "${CMAKE_SOURCE_DIR}/assets/icons/sets" "${file_path}")
	set(RES_CONTENT "${RES_CONTENT}        <file alias=\"${alias}\">${file_path}</file>\n")
endforeach()
set(RES_CONTENT "${RES_CONTENT}    </qresource>\n")

## in-app help: the user guide pages, aliased by their path below docs/user so
## relative Markdown links resolve the same way in QTextBrowser and in MkDocs
file(GLOB_RECURSE HELP_FILES CONFIGURE_DEPENDS docs/user/*.md docs/user/*.png docs/user/*.svg docs/user/*.jpg)
list(SORT HELP_FILES)
set(RES_CONTENT "${RES_CONTENT}    <qresource prefix=\"/Help\">\n")
foreach(file_path IN LISTS HELP_FILES)
	file(RELATIVE_PATH alias "${CMAKE_SOURCE_DIR}/docs/user" "${file_path}")
	set(RES_CONTENT "${RES_CONTENT}        <file alias=\"${alias}\">${file_path}</file>\n")
endforeach()
set(RES_CONTENT "${RES_CONTENT}    </qresource>\n")

## written only when the content changed, so a reconfigure doesn't rebuild the resources
file(WRITE "${CMAKE_BINARY_DIR}/resources.qrc.new" "<!-- ATTENTION this file is auto generated -->\n<RCC>\n${RES_CONTENT}</RCC>\n")
configure_file("${CMAKE_BINARY_DIR}/resources.qrc.new" "${RESOURCES}" COPYONLY)

## generate header file for drivers, to make adding a driver easier
set(DRIVER_HEADER_CONTENT "#pragma once\n\n")
foreach(file_path IN LISTS DECODER_HEADER_FILES)
	get_filename_component(base_name "${file_path}" NAME)
	set(DRIVER_HEADER_CONTENT "${DRIVER_HEADER_CONTENT}#include \"device/decoders/${base_name}\"\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/decoders.h" "${DRIVER_HEADER_CONTENT}")

## find external libs
find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})

## Bluetooth LE (Victron Instant Readout) needs Qt6::Bluetooth; without it
## the BLE port is left out and everything else builds as before.
## Off by default on macOS: CoreBluetooth hides device addresses (Qt reports a
## device UUID instead), but both BLE ports find and address meters by their
## MAC address, and the bundle carries no NSBluetoothAlwaysUsageDescription.
if (APPLE)
	set(QTDMM_WITH_BLE_DEFAULT OFF)
else()
	set(QTDMM_WITH_BLE_DEFAULT ON)
endif()
option(QTDMM_WITH_BLE "Bluetooth LE port (needs Qt6 Bluetooth)" ${QTDMM_WITH_BLE_DEFAULT})
if (QTDMM_WITH_BLE)
	find_package(Qt6 COMPONENTS Bluetooth)
	if (NOT Qt6Bluetooth_FOUND)
		message(STATUS "Qt6 Bluetooth not found - building without the BLE port")
		set(QTDMM_WITH_BLE OFF)
	endif()
endif()
if (NOT QTDMM_WITH_BLE)
	list(FILTER SRC_FILES EXCLUDE REGEX "transports/ble(gatt)?\\.(h|cpp)$")
endif()
## AES-128-CTR for the encrypted Victron advertisements (public domain tiny-AES-c)
list(APPEND SRC_FILES src/3rdparty/tiny-aes/aes.c)
## ZIP writer for the XLSX/ODS export (MIT, miniz). Only deflate and CRC are
## used - spreadsheet.cpp writes the ZIP container itself - so stdio and the
## archive writer stay out (the stdio path also prints a "may not support
## large files" pragma at every build). The archive reader stays for the
## spreadsheet test, fed from memory. Source property: applies to the tests, too.
list(APPEND SRC_FILES src/3rdparty/miniz/miniz.c)
set_source_files_properties(src/3rdparty/miniz/miniz.c PROPERTIES
	COMPILE_DEFINITIONS "MINIZ_NO_STDIO;MINIZ_NO_ARCHIVE_WRITING_APIS;MINIZ_NO_TIME")

## Translations: the build only compiles the .ts files (lrelease). Updating
## them from the sources is a manual step - `cmake --build build --target
## update_translations` - because lupdate rewrites files in the source tree,
## and when two targets ran it in parallel they truncated each other's .ts.
qt_add_translation(QM_FILES ${TRANSLATIONS})
add_custom_target(update_translations
	COMMAND Qt6::lupdate -no-obsolete ${CMAKE_SOURCE_DIR}/src -ts ${TRANSLATIONS}
	COMMENT "Updating translation sources from src/"
	VERBATIM
)
qt_wrap_ui(UI_HEADERS ${UI_FILES})
qt_add_resources(RES_SOURCES ${RESOURCES})

## developer documentation: "cmake --build build --target doxygen" writes
## build/doxygen/html; the checked-in Doxyfile also works with plain "doxygen"
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
	add_custom_target(doxygen
		# the Doxyfile writes to <source>/build/doxygen; doxygen only creates the last level
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/build/doxygen
		COMMAND Doxygen::doxygen ${CMAKE_SOURCE_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Generating developer documentation with Doxygen"
		VERBATIM)
endif()

## Compiled resources (icons, help pages, .qm files) and the generated ui
## headers live in one object library, so rcc/lrelease/uic run exactly once
## no matter how many executables (the app, the tests) use them.
add_library(qtdmm_resources OBJECT ${RES_SOURCES} ${QM_FILES} ${UI_HEADERS})
target_link_libraries(qtdmm_resources PRIVATE Qt6::Core)

## hidapi: distro package on Linux/BSD (keeps DEB/RPM/AUR dependencies
## honest), built from source on Windows and macOS: no package manager to
## rely on, and on macOS it is linked in statically, so the app bundle needs
## no Homebrew and gets the same architectures and minimum macOS as the
## rest. Either way the result is one imported target.
if(WIN32 OR APPLE)
	include(FetchContent)
	FetchContent_Declare(hidapi
		GIT_REPOSITORY https://github.com/libusb/hidapi.git
		GIT_TAG hidapi-0.14.0
		GIT_SHALLOW TRUE
	)
	set(HIDAPI_WITH_TESTS OFF CACHE BOOL "" FORCE)
	set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF)
	# hidapi 0.14 still declares cmake_minimum_required 3.1.3, which CMake >= 4
	# refuses outright; this lets it configure under the 3.5 policy set.
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
	FetchContent_MakeAvailable(hidapi)
	unset(CMAKE_POLICY_VERSION_MINIMUM)
	set(HIDAPI_TARGET hidapi::hidapi)
else()
	find_package(PkgConfig REQUIRED)
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi-hidraw)
	else()
		pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
	endif()
	set(HIDAPI_TARGET PkgConfig::HIDAPI)
endif()

if(APPLE)
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.qtdmm.qtdmm")
	set(MACOSX_BUNDLE_BUNDLE_NAME "${APP_NAME}")
	set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
	# no CMAKE_OSX_ARCHITECTURES default: a local build is for the Mac's own
	# architecture (Homebrew's Qt has only that one); the CI passes it
endif()

## the sources are UTF-8 (unit strings such as "µ" and "Ω"); MSVC would
## otherwise read them in the system code page and mangle those literals
if(MSVC)
	add_compile_options(/utf-8)
endif()

## compile the application
add_definitions(-DAPP_VERSION="${QTDMM_VERSION}" -DAPP_NAME="${APP_NAME}" -DAPP_ORGANIZATION="${APP_ORGANIZATION}" )
include_directories(src ${CMAKE_BINARY_DIR})

if(WIN32)
	enable_language(RC)
	set(WIN_RESOURCES assets/windows/qtdmm.rc) # executable icon
endif()

if(APPLE)
	# bundle icon: copied to qtdmm.app/Contents/Resources, named in Info.plist
	# (regenerate with tools/make_icons.py from assets/icons/app/qtdmm.svg)
	set(MAC_RESOURCES assets/macos/qtdmm.icns)
	set_source_files_properties(${MAC_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

add_executable(${PROJECT_NAME}
	MACOSX_BUNDLE
	WIN32
	${SRC_FILES}
	${WIN_RESOURCES}
	${MAC_RESOURCES}
)
if(APPLE)
	set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE qtdmm.icns)
endif()

# paranoia options, force you to correct all compiler warnings
# (compile.sh turns this on for ctest builds, so CI fails on a new warning)
option(QTDMM_WERROR "Treat compiler warnings in QtDMM's own code as errors" OFF)
if(QTDMM_WERROR)
	# bundled third-party code keeps its warnings; they are not ours to fix
	set(_third_party src/3rdparty/miniz/miniz.c src/3rdparty/tiny-aes/aes.c)
	if(MSVC)
		set(QTDMM_WARNING_FLAGS /W4 /WX)
		set_source_files_properties(${_third_party}
			PROPERTIES COMPILE_OPTIONS "/WX-")
	else()
		set(QTDMM_WARNING_FLAGS -Wall -Werror) # -Wextra -Wpedantic
		set_source_files_properties(${_third_party}
			PROPERTIES COMPILE_OPTIONS "-Wno-error")
	endif()
	# the tests get the same flags (cmake/ctest.cmake)
	target_compile_options(${PROJECT_NAME} PRIVATE ${QTDMM_WARNING_FLAGS})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
	qtdmm_resources
	Qt6::Widgets
	Qt6::SerialPort
	Qt6::PrintSupport
	Qt6::Network
	Qt6::Charts
	Qt6::Svg
	${HIDAPI_TARGET}
	$<$<BOOL:${QTDMM_WITH_BLE}>:Qt6::Bluetooth>
)
if (QTDMM_WITH_BLE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE QTDMM_WITH_BLE)
endif()

## install files
install(TARGETS ${PROJECT_NAME}
	BUNDLE DESTINATION .
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
)

if (NOT WIN32)
	## man page with this build's version and commit date; gzip -n leaves the
	## time stamp out, so the same commit gives the same file
	set(QTDMM_MAN_DATE "${QTDMM_COMMIT_YEAR}-${QTDMM_COMMIT_MONTH}-${QTDMM_COMMIT_DAY}")
	string(REGEX REPLACE "-([0-9])(-|$)" "-0\\1\\2" QTDMM_MAN_DATE "${QTDMM_MAN_DATE}")
	string(REGEX REPLACE "-([0-9])$" "-0\\1" QTDMM_MAN_DATE "${QTDMM_MAN_DATE}")
	configure_file(${CMAKE_SOURCE_DIR}/docs/qtdmm.1.in ${CMAKE_BINARY_DIR}/qtdmm.1 @ONLY)
	execute_process( COMMAND gzip -n -f ${CMAKE_BINARY_DIR}/qtdmm.1 )

	## desktop file and icon are named after the application id, like the
	## AppStream component (freedesktop recommends it, Flathub requires it)
	install(FILES assets/io.github.qtdmm.qtdmm.desktop DESTINATION share/applications)
	install(FILES assets/icons/app/qtdmm_32.png DESTINATION share/icons/hicolor/32x32/apps RENAME io.github.qtdmm.qtdmm.png)
	install(FILES assets/icons/app/qtdmm_64.png DESTINATION share/icons/hicolor/64x64/apps RENAME io.github.qtdmm.qtdmm.png)
	install(FILES assets/icons/app/qtdmm_128.png DESTINATION share/icons/hicolor/128x128/apps RENAME io.github.qtdmm.qtdmm.png)
	install(FILES assets/icons/app/qtdmm.svg DESTINATION share/icons/hicolor/scalable/apps RENAME io.github.qtdmm.qtdmm.svg)
	install(FILES ${CMAKE_BINARY_DIR}/qtdmm.1.gz DESTINATION share/man/man1)
	## AppStream metadata for software centres, named after the component id
	install(FILES assets/appimage/qtdmm.appdata.xml DESTINATION share/metainfo RENAME io.github.qtdmm.qtdmm.metainfo.xml)
endif()

## main.cpp looks for them next to the binary: <prefix>/bin/../share/qtdmm/translations
if (NOT QTDMM_EMBED_TRANSLATIONS)
	install(FILES ${QM_FILES} DESTINATION share/qtdmm/translations)
endif()

## include test framework
include(cmake/ctest.cmake)

## include deployment
include(cmake/deploy.cmake)
