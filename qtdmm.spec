%bcond test 1
%global	toolchain gcc
# application id: desktop file, icons and AppStream component
%global	appid io.github.qtdmm.qtdmm

Name:		qtdmm
Version:	26.4
# Für RC's -p -e rcX
# Sonst -p -e weglassen
Release:	%autorelease -p -e rc1
Summary:	DMM Readout Software Including a Configurable Recorder
License:	GPL-3.0-or-later AND LGPL-3.0-or-later
URL:		https://www.qtdmm.de
BuildSystem:	cmake
%if %{with test}
BuildOption:	-DBUILD_TESTING=ON
%endif
# Übersetzungen als eigene Dateien für %%find_lang
BuildOption:	-DQTDMM_EMBED_TRANSLATIONS=OFF
# Release-Tag des Quell-Archivs; für RC's %%{version}-rcX
%global	tag %{version}-rc1
# Copr (.copr/Makefile) setzt hier sein eigenes Archiv aus dem Checkout ein
Source0:	https://github.com/qtdmm/QtDMM/releases/download/%{tag}/QtDMM-%{tag}.tar.gz
BuildRequires:	appdata-tools desktop-file-utils gcc-c++
BuildRequires:	cmake(Qt6Bluetooth) cmake(Qt6Charts) cmake(Qt6LinguistTools) cmake(Qt6Multimedia) cmake(Qt6SerialPort) cmake(Qt6Svg)
BuildRequires:	cmake(hidapi)
BuildRequires:	pkgconfig(cups)
Suggests:	sigrok-cli

%description
QtDMM is a graphical multimeter reader and logger based on Qt.
It reads more than 150 digital multimeters over serial and USB cables,
Bluetooth LE and the network.

%install
%cmake_install
# the licence texts go to %%{_licensedir} by %%license below
rm -rf %{buildroot}%{_datadir}/licenses/%{name}
%find_lang %{name} --with-qt

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{appid}.desktop
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/*.metainfo.xml
QT_QPA_PLATFORM=offscreen %ctest

%files -f %{name}.lang
%license LICENSE assets/icons/sets/LICENSE.breeze assets/icons/sets/LICENSE.oxygen
%doc AUTHORS README.md CHANGELOG.md
%{_bindir}/%{name}
%{_datadir}/applications/%{appid}.desktop
%{_datadir}/icons/hicolor/*/apps/%{appid}.png
%{_datadir}/icons/hicolor/scalable/apps/%{appid}.svg
%{_mandir}/man1/%{name}.1*
%{_prefix}/lib/udev/rules.d/70-qtdmm.rules
%{_metainfodir}/*.xml
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/translations

%changelog
%autochangelog
