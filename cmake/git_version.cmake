# Copyright (c) 2025 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later

# get version
#
# QtDMM versions are YY.N[.P][-rcK] (calendar versioning): YY the two-digit year,
# N the release number within that year, P a bugfix release of YY.N, -rcK /
# -betaK / -alphaK a pre-release of YY.N. Releases are git tags with exactly
# that name ("26.1-rc1", "26.1", "26.1.1"); docs/dev/releasing.md has the
# steps.
#
# Where the version comes from, first match wins:
#   1. git describe on the newest such tag (a clone; CI must fetch tags)
#   2. .tarball-version, written into the source tarball by CPack
#   3. .archive-version, filled in by "git archive" (GitHub's source
#      downloads) via export-subst in .gitattributes. Not "VERSION": the
#      source root is on the include path, and on case-insensitive file
#      systems (Windows, macOS) <version> from the C++ library would find it.
#   4. nothing: 0.0-unknown
#
# A commit that is not tagged is a development build of the next release:
#   26.1 + 14 commits      -> 26.2-dev.14+g1234abc  (27.1-dev... once the commit is from 2027)
#   26.1-rc1 + 3 commits   -> 26.1-rc1.3+g1234abc   (after rc1, before the release)
#   26.4-dev + 7 commits   -> 26.4-dev.7+g1234abc   (master while 26.3 has a branch of its own)
#   no tag at all          -> <YY of the commit>.1-dev.<commits>+g...
#
# A YY.N-dev tag marks where master heads for YY.N while YY.(N-1) is finished
# on its release branch. git describe takes the nearest tag, and a merge of
# the release branch can bring its rc tag nearer: the version is the higher
# of the nearest release tag's and the nearest -dev tag's (qtdmm_pick_describe).
#
# Results (PARENT_SCOPE):
#   QTDMM_VERSION           shown in the UI, --version, *IDN?  (26.2-dev.14+g1234abc)
#   QTDMM_PACKAGE_VERSION   RPM/DEB: '~' sorts a pre-release before the release,
#                           '+' is not allowed                  (26.2~dev.14.g1234abc)
#   PROJECT_VERSION[_MAJOR|_MINOR|_PATCH]
#                           numbers only, for CMake, CPack, the macOS bundle and
#                           Windows version resources            (26.2.0)
#   QTDMM_COMMIT_YEAR/_MONTH/_DAY  date of the commit (man page)
#
# qtdmm_parse_describe() is also what tests/test_version.cmake checks.

## @p describe: a tag, or "<tag>-<n>-g<hash>" (git describe --long), or ""
## when there is no tag; @p count / @p hash are used for "" only.
## Sets <prefix>_DISPLAY, <prefix>_PACKAGE, <prefix>_MAJOR/_MINOR/_PATCH,
## <prefix>_ERROR (empty when fine) in the caller's scope.
function(qtdmm_parse_describe describe commit_yy count hash prefix)
	set(_err "")
	set(_tag "")
	set(_n 0)
	set(_h "${hash}")
	if (describe MATCHES "^(.+)-([0-9]+)-g([0-9a-f]+)$")
		set(_tag "${CMAKE_MATCH_1}")
		set(_n "${CMAKE_MATCH_2}")
		set(_h "${CMAKE_MATCH_3}")
	elseif (NOT describe STREQUAL "")
		set(_tag "${describe}")
	else()
		set(_n "${count}")
	endif()

	if (_tag MATCHES "^([0-9][0-9])\\.([1-9][0-9]*)-dev$")
		# master heading for YY.N while the release before it has a branch
		set(_yy ${CMAKE_MATCH_1})
		set(_minor ${CMAKE_MATCH_2})
		if (commit_yy GREATER _yy)
			set(_yy ${commit_yy})
			set(_minor 1)
		endif()
		set(_patch 0)
		set(_display "${_yy}.${_minor}-dev.${_n}+g${_h}")
		set(_package "${_yy}.${_minor}~dev.${_n}.g${_h}")
	elseif (_tag STREQUAL "")
		# no release yet: working towards the first one of the commit's year
		set(_yy ${commit_yy})
		set(_minor 1)
		set(_patch 0)
		set(_display "${_yy}.1-dev.${_n}+g${_h}")
		set(_package "${_yy}.1~dev.${_n}.g${_h}")
	elseif (_tag MATCHES "^([0-9][0-9])\\.([1-9][0-9]*)(\\.([0-9]+))?(-(alpha|beta|rc)([1-9][0-9]*))?$")
		set(_yy ${CMAKE_MATCH_1})
		set(_minor ${CMAKE_MATCH_2})
		set(_patch "${CMAKE_MATCH_4}")
		if ("${_patch}" STREQUAL "")
			set(_patch 0)
		endif()
		set(_pre "${CMAKE_MATCH_6}${CMAKE_MATCH_7}")
		if (_n EQUAL 0)
			set(_display "${_tag}")
			string(REPLACE "-" "~" _package "${_tag}")
		elseif (NOT _pre STREQUAL "")
			# after a pre-release tag: still heading for that release
			set(_display "${_tag}.${_n}+g${_h}")
			string(REPLACE "-" "~" _package "${_tag}.${_n}.g${_h}")
		else()
			# after a release: the next one, a new year starts at .1
			if (commit_yy GREATER _yy)
				set(_yy ${commit_yy})
				set(_minor 1)
			else()
				math(EXPR _minor "${_minor} + 1")
			endif()
			set(_patch 0)
			set(_display "${_yy}.${_minor}-dev.${_n}+g${_h}")
			set(_package "${_yy}.${_minor}~dev.${_n}.g${_h}")
		endif()
	else()
		set(_err "'${_tag}' is not a version tag (YY.N[.P][-rcK])")
		set(_yy 0)
		set(_minor 0)
		set(_patch 0)
		set(_display "0.0-unknown")
		set(_package "0.0~unknown")
	endif()

	# 26 -> 26, but "05" -> 5 for the numeric fields
	string(REGEX REPLACE "^0+([0-9])" "\\1" _yy "${_yy}")
	set(${prefix}_DISPLAY "${_display}" PARENT_SCOPE)
	set(${prefix}_PACKAGE "${_package}" PARENT_SCOPE)
	set(${prefix}_MAJOR "${_yy}" PARENT_SCOPE)
	set(${prefix}_MINOR "${_minor}" PARENT_SCOPE)
	set(${prefix}_PATCH "${_patch}" PARENT_SCOPE)
	set(${prefix}_ERROR "${_err}" PARENT_SCOPE)
endfunction()

## Of @p release (git describe on the release tags) and @p dev (on the -dev
## tags), the one heading for the higher version; release when they are
## equal (an rc on the release branch, which has the -dev tag behind it).
## Sets <out> in the caller's scope.
function(qtdmm_pick_describe release dev commit_yy out)
	set(_pick "${release}")
	if (NOT dev STREQUAL "")
		qtdmm_parse_describe("${release}" "${commit_yy}" 0 x R)
		qtdmm_parse_describe("${dev}" "${commit_yy}" 0 x D)
		if (release STREQUAL "" OR NOT R_ERROR STREQUAL ""
		    OR "${D_MAJOR}.${D_MINOR}.${D_PATCH}" VERSION_GREATER "${R_MAJOR}.${R_MINOR}.${R_PATCH}")
			set(_pick "${dev}")
		endif()
	endif()
	set(${out} "${_pick}" PARENT_SCOPE)
endfunction()

function(get_version_from_git)
	set(_describe "")
	set(_count 0)
	set(_hash "unknown")
	set(_source "")
	string(TIMESTAMP _date "%Y.%m.%d")   # no git: today, only for the RPM changelog

	# the tag pattern leaves out the old 0.9.x and 1.0.0-alpha.1 tags
	set(_match "[0-9][0-9].[1-9]*")
	find_package(Git QUIET)
	if (Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
		execute_process(
			COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=format:%Y.%m.%d
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE _gitdate OUTPUT_STRIP_TRAILING_WHITESPACE
			RESULT_VARIABLE _r)
		if (_r EQUAL 0)
			set(_date "${_gitdate}")
			execute_process(
				COMMAND ${GIT_EXECUTABLE} describe --tags --long --abbrev=7 --match "${_match}" --exclude "*-dev"
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
				OUTPUT_VARIABLE _describe OUTPUT_STRIP_TRAILING_WHITESPACE
				RESULT_VARIABLE _r ERROR_QUIET)
			if (NOT _r EQUAL 0)
				set(_describe "")   # no version tag reachable (or a shallow clone)
			endif()
			execute_process(
				COMMAND ${GIT_EXECUTABLE} describe --tags --long --abbrev=7 --match "${_match}-dev"
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
				OUTPUT_VARIABLE _dev OUTPUT_STRIP_TRAILING_WHITESPACE
				RESULT_VARIABLE _r ERROR_QUIET)
			if (_r EQUAL 0)
				string(SUBSTRING "${_gitdate}" 2 2 _gityy)
				qtdmm_pick_describe("${_describe}" "${_dev}" "${_gityy}" _describe)
			endif()
			execute_process(
				COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
				OUTPUT_VARIABLE _count OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
			execute_process(
				COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
				OUTPUT_VARIABLE _hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
			set(_source "git")
		endif()
	endif()
	if (_source STREQUAL "" AND EXISTS "${CMAKE_SOURCE_DIR}/.tarball-version")
		# lines: git describe --long (may be empty), commit date, commit count, hash
		file(READ "${CMAKE_SOURCE_DIR}/.tarball-version" _content)
		string(REPLACE "\n" ";" _lines "${_content}")
		list(LENGTH _lines _len)
		if (_len GREATER 3)
			list(GET _lines 0 _describe)
			list(GET _lines 1 _date)
			list(GET _lines 2 _count)
			list(GET _lines 3 _hash)
		endif()
		set(_source ".tarball-version")
	endif()
	if (_source STREQUAL "" AND EXISTS "${CMAKE_SOURCE_DIR}/.archive-version")
		file(STRINGS "${CMAKE_SOURCE_DIR}/.archive-version" _lines)
		list(GET _lines 0 _describe)
		if (_describe MATCHES "^\\$Format")
			set(_describe "")   # not an archive: git did not fill it in
		else()
			list(LENGTH _lines _len)
			if (_len GREATER 1)
				list(GET _lines 1 _date)
			endif()
			set(_source ".archive-version")
		endif()
	endif()

	# YYYY.MM.DD from git log, YYYY-MM-DD from git archive (%cs)
	string(REGEX MATCH "^[0-9][0-9]([0-9][0-9])[.-]([0-9]+)[.-]([0-9]+)" _ "${_date}")
	set(_cyy "${CMAKE_MATCH_1}")
	set(_cm "${CMAKE_MATCH_2}")
	set(_cd "${CMAKE_MATCH_3}")
	string(SUBSTRING "${_date}" 0 4 _cyear)

	if (_source STREQUAL "")
		message(WARNING "No git, no .tarball-version, no .archive-version: version unknown")
		set(V_DISPLAY "0.0-unknown")
		set(V_PACKAGE "0.0~unknown")
		set(V_MAJOR 0)
		set(V_MINOR 0)
		set(V_PATCH 0)
	else()
		qtdmm_parse_describe("${_describe}" "${_cyy}" "${_count}" "${_hash}" V)
		if (NOT V_ERROR STREQUAL "")
			message(WARNING "${V_ERROR}")
		endif()
	endif()

	set(QTDMM_VERSION "${V_DISPLAY}" PARENT_SCOPE)
	set(QTDMM_PACKAGE_VERSION "${V_PACKAGE}" PARENT_SCOPE)
	# for .tarball-version (cmake/deploy.cmake)
	set(QTDMM_TARBALL_VERSION "${_describe}\n${_date}\n${_count}\n${_hash}\n" PARENT_SCOPE)
	set(PROJECT_VERSION_MAJOR ${V_MAJOR} PARENT_SCOPE)
	set(PROJECT_VERSION_MINOR ${V_MINOR} PARENT_SCOPE)
	set(PROJECT_VERSION_PATCH ${V_PATCH} PARENT_SCOPE)
	set(PROJECT_VERSION "${V_MAJOR}.${V_MINOR}.${V_PATCH}" PARENT_SCOPE)
	# strip leading zeros (CMake/CPack/RPM want plain integers, e.g. "9" not "09")
	string(REGEX REPLACE "^0+([0-9])" "\\1" _cm "${_cm}")
	string(REGEX REPLACE "^0+([0-9])" "\\1" _cd "${_cd}")
	set(QTDMM_COMMIT_YEAR "${_cyear}" PARENT_SCOPE)
	set(QTDMM_COMMIT_MONTH "${_cm}" PARENT_SCOPE)
	set(QTDMM_COMMIT_DAY "${_cd}" PARENT_SCOPE)

	message(STATUS "QtDMM version: ${V_DISPLAY} (package ${V_PACKAGE}, from ${_source})")
endfunction()

if (NOT QTDMM_VERSION_TEST)
	get_version_from_git()
endif()

# cmake -P cmake/git_version.cmake prints the version alone (the Pages
# workflow puts master's into latest.json for the update check)
if (CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE AND NOT QTDMM_VERSION_TEST)
	execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${QTDMM_VERSION}")
endif()
