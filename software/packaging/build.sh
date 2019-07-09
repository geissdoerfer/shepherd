#!/bin/bash
set -e

build_firmware() {
    cd /code/packaging/shepherd-firmware

    ln -s /code/firmware firmware
    ln -s /code/device-tree device-tree

    dpkg-buildpackage -uc -us

    cp ../shepherd-firmware_*_all.deb /artifacts/debian/
}

build_kernel_module() {
    cd /code/packaging/shepherd-kernel-dkms

    ln -s /code/kernel-module src

    dpkg-buildpackage -uc -us

    cp ../shepherd-dkms_*_all.deb /artifacts/debian/
}

build_python_package() {
    cd /code/packaging/python3-shepherd

    ln -s /code/python-package python-package

    cd python-package

    python3 setup.py --command-packages=stdeb.command sdist_dsc

    cp /code/packaging/python3-shepherd/debian/* deb_dist/shepherd-*/debian/

    cd deb_dist/shepherd-*

    dpkg-buildpackage -uc -us

    cp ../python3-shepherd_*_all.deb /artifacts/debian/
}

build_openocd() {
    cd /code/packaging/shepherd-openocd
    git clone --depth 1 http://openocd.zylin.com/openocd
    cd openocd
    git submodule update --init
    git fetch http://openocd.zylin.com/openocd refs/changes/71/4671/2 && git checkout FETCH_HEAD
    cp -r ../debian ../shepherd.cfg ./

    dpkg-buildpackage -uc -us
    cp ../shepherd-openocd_*_all.deb /artifacts/debian/
}

build_ptp() {
    cd /code/packaging/shepherd-ptp

    dpkg-buildpackage -uc -us
    cp ../shepherd-ptp_*_all.deb /artifacts/debian/
}

build_meta_package() {
    cd /code/packaging/shepherd
    equivs-build ns-control

    cp shepherd_*_all.deb /artifacts/debian/
}

make_repository() {
    cd /artifacts
    dpkg-scanpackages debian /dev/null | gzip -9c > debian/Packages.gz
}

mkdir -p /artifacts/debian

build_ptp
build_openocd
build_firmware
build_kernel_module
build_python_package
build_meta_package
make_repository