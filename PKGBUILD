# Maintainer: user14923929 <user14923929@users.noreply.github.com>
pkgname=flashpack
pkgver=0.1.0
pkgrel=1
pkgdesc="Cross-platform Qt6 GUI for adb and fastboot"
arch=('x86_64' 'aarch64')
url="https://github.com/user14923929/flashpack"
license=('GPL-3.0-or-later')
depends=('qt6-base' 'android-tools')
makedepends=('cmake')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cmake -B build -S "$pkgname-$pkgver" \
        -DCMAKE_BUILD_TYPE=None \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
