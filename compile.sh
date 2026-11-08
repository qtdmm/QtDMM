#!/bin/sh

usage()
{
cat << EOF

usage: compile.sh <install|clean|qt6>

 builds QtDMM. Additional options:

   appimage: creates appImage
   clean   : remove build files before build
   ctest   : build and run ctest
   doxygen : generate the developer documentation (build/doxygen/html)
   install : install system wide
   pack    : create packages (DEB and source)
   run     : run qtdmm after successfull build

 Ninja and ccache are used when installed (Ninja only for a new build
 directory: "clean" switches an existing one over).

 Another Qt than the system's: QTDMM_QT_DIR=~/Qt/6.8.3/gcc_64 ./compile.sh
 (switching between Qts needs "clean").

EOF
	exit 0
}

RUN=false
INSTALL=false
PACK=false
CTEST=false
APPIMG=false
DOXYGEN=false

for arg in $*
do
	arg=$(echo "$arg" | tr '[:upper:]' '[:lower:]')
	[ "$arg" = "clean"    ] && rm -rf build
	[ "$arg" = "ctest"    ] && CTEST=true
	[ "$arg" = "install"  ] && INSTALL=true
	[ "$arg" = "run"      ] && RUN=true
	[ "$arg" = "pack"     ] && PACK=true
	[ "$arg" = "appimage" ] && APPIMG=true
	[ "$arg" = "doxygen"  ] && DOXYGEN=true
	[ "$arg" = "help"     ] && usage
done

if [ "$(uname)" = "Linux" ] >/dev/null
then
	JOBS=$(nproc)
	CMAKE_PARAMS="-DCMAKE_INSTALL_PREFIX=/usr"
elif [ "$(uname)" = "FreeBSD" ] >/dev/null
then
	JOBS=$(sysctl -n hw.ncpu)
else
	JOBS=$(sysctl -n hw.ncpu)
	CMAKE_PARAMS="-DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)"
fi

# another Qt than the system's (or Homebrew's): the directory with its bin/
# and lib/cmake/. A build directory keeps the Qt it was set up with (CMake
# caches the paths): a switch builds afresh after "clean" only
QT_STAMP=build/.qtdmm-qt-dir
if [ -f build/CMakeCache.txt ] && [ "$(cat ${QT_STAMP} 2>/dev/null)" != "${QTDMM_QT_DIR}" ]
then
	echo "build/ was set up with another Qt (QTDMM_QT_DIR \"$(cat ${QT_STAMP} 2>/dev/null)\"): ./compile.sh clean"
	exit 1
fi
if [ -n "${QTDMM_QT_DIR}" ]
then
	CMAKE_PARAMS="${CMAKE_PARAMS} -DCMAKE_PREFIX_PATH=${QTDMM_QT_DIR}"
fi

# Ninja and ccache when installed: the tests build many sources again, a
# rebuild after clean or a branch switch then mostly comes from the cache. An
# existing build directory keeps its generator (cmake refuses to change it)
if [ ! -f build/CMakeCache.txt ] && command -v ninja >/dev/null
then
	CMAKE_PARAMS="${CMAKE_PARAMS} -G Ninja"
fi
if command -v ccache >/dev/null
then
	CMAKE_PARAMS="${CMAKE_PARAMS} -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
fi

cmake ${CMAKE_PARAMS} -DBUILD_TESTING=$(${CTEST} && echo "ON" || echo "OFF") \
	-DQTDMM_WERROR=$(${CTEST} && echo "ON" || echo "OFF") -B build || exit 1
echo "${QTDMM_QT_DIR}" > ${QT_STAMP}
cmake --build build --parallel ${JOBS} || exit 1

cd build

if ${PACK}
then
	rm -rf ../packages/
	cpack --config CPackSourceConfig.cmake

	if [ -f /etc/debian_version ]
	then
		cpack --config CPackConfig.cmake
	elif [ "$(uname)" = "Darwin" ]
	then
		echo "mac osx packages not supported yet"
	else
		echo "unsupported OS. no package creation"
	fi
	rm -rf ../packages/_CPack_Packages
fi

if ${CTEST}
then
	echo
	ctest --test-dir . --output-on-failure --timeout 300 || exit 1
	echo
fi

if ${DOXYGEN}
then
	if command -v doxygen >/dev/null
	then
		cmake --build . --target doxygen || exit 1
		echo "developer documentation: build/doxygen/html/index.html"
	else
		echo "doxygen not installed, skipping developer documentation"
	fi
fi

QTDMM_EXE="qtdmm"
[ "$(uname)" = "Darwin" ] && QTDMM_EXE="qtdmm.app/Contents/MacOS/qtdmm"

if [ ! -x ${QTDMM_EXE} ]
then
	echo "build/qtdmm not found"
	exit 1
fi

if [ "$(uname)" = "Linux" ] && ${APPIMG}
then
	# linuxdeploy with its Qt plugin: the libraries the binary needs (not glibc
	# and the others every system has), the Qt plugins (platforms, the SVG
	# symbols, image formats, TLS) and a RUNPATH to them - a plain copy of
	# ldd's list was never used, the AppImage ran on the host's Qt
	rm -rf AppDir ../packages/QtDMM.AppImage
	for tool in linuxdeploy/linuxdeploy-x86_64 linuxdeploy-plugin-qt/linuxdeploy-plugin-qt-x86_64
	do
		file="$(basename ${tool}).AppImage"
		if [ ! -x "${file}" ]
		then
			# a failed download says so: wget -q alone ends the build without a word
			url="https://github.com/linuxdeploy/$(dirname ${tool})/releases/download/continuous/${file}"
			if ! wget -q --https-only --tries=3 --waitretry=10 --retry-on-http-error=502,503,504 "${url}"
			then
				echo "download failed: ${url}" >&2
				rm -f "${file}"
				exit 1
			fi
			chmod +x "${file}"
		fi
	done
	DESTDIR=AppDir cmake --install .
	# appimagetool looks the metadata up by the desktop file's name (the
	# application id), as .appdata.xml
	mv AppDir/usr/share/metainfo/io.github.qtdmm.qtdmm.metainfo.xml AppDir/usr/share/metainfo/io.github.qtdmm.qtdmm.appdata.xml
	# appimagetool (since 2026-10-31) wants a PNG at the top of the AppDir for
	# .DirIcon, the SVG linuxdeploy puts there no longer does
	cp AppDir/usr/share/icons/hicolor/128x128/apps/io.github.qtdmm.qtdmm.png AppDir/ || exit 1
	# a build host without FUSE (containers, CI) runs the tools extracted
	export APPIMAGE_EXTRACT_AND_RUN=1
	[ -n "${QTDMM_QT_DIR}" ] && QMAKE="${QMAKE:-${QTDMM_QT_DIR}/bin/qmake}"
	export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
	export EXTRA_QT_PLUGINS="svg"
	export LDAI_OUTPUT=QtDMM.AppImage
	export LDAI_NO_APPSTREAM=1
	ARCH=x86_64 ./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage \
		--desktop-file AppDir/usr/share/applications/io.github.qtdmm.qtdmm.desktop \
		--icon-file AppDir/usr/share/icons/hicolor/scalable/apps/io.github.qtdmm.qtdmm.svg || exit 1
	rm -rf AppDir
	mkdir -p ../packages
	mv QtDMM.AppImage ../packages
fi


mkdir -p ../bin
cp ${QTDMM_EXE} qtdmm*.qm ../bin

if [ "$(uname)" != "Darwin" ] && ${INSTALL}
then
	echo
	echo "-- install QtDMM system wide --"
	SUDO=""
	[ "$(id -u)" -ne 0 ] && SUDO="$(command -v sudo || command -v doas)"
	${SUDO} cmake --install . || exit 1
	${RUN} && ${QTDMM_EXE}
elif ${RUN}
then
	./${QTDMM_EXE} #--debug
fi

exit 0
