# Maintainer: bebsh
pkgname=rog-ally-menu
pkgver=0.2.0
pkgrel=1
pkgdesc="ROG Ally overlay menu for Linux - replicates the Windows ROG Command Center"
arch=('x86_64')
license=('MIT')
depends=(
    'qt6-base'
    'qt6-declarative'
    'layer-shell-qt'
    'polkit'
    'grim'
)
makedepends=(
    'cmake'
)
source=()
sha256sums=()

build() {
    cd "$startdir"
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    cd "$startdir"
    DESTDIR="$pkgdir" cmake --install build
}
