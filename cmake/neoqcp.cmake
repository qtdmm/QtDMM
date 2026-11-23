# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later

## NeoQCP (QTDMM_WITH_NEOQCP): its sources in QTDMM_NEOQCP_DIR, built as a
## static library of this build. Included before the sources are listed.

if (NOT EXISTS "${QTDMM_NEOQCP_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "QTDMM_WITH_NEOQCP: no NeoQCP sources in ${QTDMM_NEOQCP_DIR}.\n"
		"\"./compile.sh neoqcp\" fetches them, or set QTDMM_NEOQCP_DIR to a checkout of "
		"https://github.com/SciQLop/NeoQCP")
endif()
if (Qt6_VERSION VERSION_LESS 6.7)
	message(FATAL_ERROR "QTDMM_WITH_NEOQCP: NeoQCP needs Qt 6.7 or newer, this is Qt ${Qt6_VERSION}")
endif()

## NeoQCP uses two things of Qt 6.9 (the GuiPrivate package, QImage::flipped()).
## For Qt 6.7 and 6.8 the checkout gets cmake/neoqcp-qt6.8.patch, once. The
## patch is made for the revision compile.sh fetches; another one may refuse it.
if (Qt6_VERSION VERSION_LESS 6.9)
	find_package(Git QUIET)
	set(_patch "${CMAKE_SOURCE_DIR}/cmake/neoqcp-qt6.8.patch")
	execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_patch}"
		WORKING_DIRECTORY "${QTDMM_NEOQCP_DIR}" RESULT_VARIABLE _applied OUTPUT_QUIET ERROR_QUIET)
	if (NOT _applied EQUAL 0)
		execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${_patch}"
			WORKING_DIRECTORY "${QTDMM_NEOQCP_DIR}" RESULT_VARIABLE _failed ERROR_VARIABLE _why)
		if (NOT _failed EQUAL 0)
			message(FATAL_ERROR "QTDMM_WITH_NEOQCP: cmake/neoqcp-qt6.8.patch does not apply to "
				"${QTDMM_NEOQCP_DIR} (needed for Qt ${Qt6_VERSION}, older than 6.9):\n${_why}")
		endif()
		message(STATUS "NeoQCP: patched for Qt ${Qt6_VERSION}")
	endif()
endif()

## without LaTeX in labels: QtDMM has none, and it would fetch JKQTMathText
set(NEOQCP_WITH_LATEX OFF CACHE BOOL "" FORCE)
add_subdirectory("${QTDMM_NEOQCP_DIR}" "${CMAKE_BINARY_DIR}/neoqcp" EXCLUDE_FROM_ALL)
message(STATUS "QCustomPlot views: NeoQCP from ${QTDMM_NEOQCP_DIR}")
