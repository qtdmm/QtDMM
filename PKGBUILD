# Maintainer: QtDMM team <hello@qtdmm.de>
pkgname=qtdmm
pkgver=0.0.0 # placeholder, overwritten by pkgver() below on every build
pkgrel=1
pkgdesc="Read out and record digital multimeters"
arch=('x86_64')
url="https://github.com/qtdmm/QtDMM"
license=('GPL-3.0-or-later' 'LGPL-3.0-or-later')
provides=('qtdmm')
conflicts=('qtdmm' 'qtdmm-qt5')
depends=('qt6-base' 'qt6-translations' 'qt6-serialport' 'qt6-svg' 'qt6-connectivity' 'qt6-multimedia' 'hidapi')
optdepends=('sigrok-cli: bench meters through sigrok')
makedepends=('qt6-tools' 'cmake' 'git')
source=("$pkgname::git+https://github.com/qtdmm/QtDMM.git")
sha256sums=('SKIP')


pkgver() {
  cd $pkgname
  # QtDMM tags releases YY.N[.P][-rcK] (cmake/git_version.cmake); Arch's usual
  # VCS form on top: 26.1.r14.g1234abc after the tag 26.1, 26.1rc1.r3.g... after
  # 26.1-rc1 (pacman sorts 26.1rc1 before 26.1). Without a tag yet: 0.r<count>.g<hash>.
  local d
  if d=$(git describe --tags --long --abbrev=7 --match '[0-9][0-9].[1-9]*' 2>/dev/null); then
    echo "$d" | sed -E 's/-(alpha|beta|rc)/\1/; s/-([0-9]+)-g/.r\1.g/'
  else
    printf '0.r%s.g%s\n' "$(git rev-list --count HEAD)" "$(git rev-parse --short=7 HEAD)"
  fi
}

build() {
  cd "$pkgname"
  ./compile.sh
}

package() {
  cd "$pkgname"
  DESTDIR="$pkgdir" cmake --install build
}
