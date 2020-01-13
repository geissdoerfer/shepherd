#!/bin/bash
# This script builds all the shepherd debian packages and archives them for
# hosting as third-party repository. It is meant to be used inside a docker
# build container
set -e

build_firmware() {
    cd /code/firmware
    dpkg-buildpackage -uc -us

    cp ../shepherd-firmware_*_all.deb /artifacts/debian/
}

build_kernel_module() {
    cd /code/kernel-module
    dpkg-buildpackage -uc -us

    cp ../shepherd-dkms_*_all.deb /artifacts/debian/
}

build_python_package() {
    cd /code/python-package

    python3 setup.py sdist
    dpkg-buildpackage -uc -us

    cp ../python3-shepherd_*_all.deb /artifacts/debian/
}

build_openocd() {
    cd /code/openocd
    git clone --depth 1 http://openocd.zylin.com/openocd
    cd openocd
    git submodule update --init
    git fetch http://openocd.zylin.com/openocd refs/changes/71/4671/2 && git checkout FETCH_HEAD
    cp -r ../debian ../shepherd.cfg ./

    dpkg-buildpackage -uc -us
    cp ../shepherd-openocd_*_all.deb /artifacts/debian/
}

build_ptp() {
    cd /code/ptp

    dpkg-buildpackage -uc -us
    cp ../shepherd-ptp_*_all.deb /artifacts/debian/
}

build_pps_gmtimer() {
    cd /code/pps_gmtimer
    dpkg-buildpackage -uc -us

    cp ../pps-gmtimer-dkms_*_all.deb /artifacts/debian/
}

build_meta_package() {
    cd /code/meta-package

    dpkg-buildpackage -uc -us
    cp ../shepherd_*_all.deb /artifacts/debian/
}

make_repository() {
    cd /artifacts
    dpkg-scanpackages debian /dev/null | gzip -9c > debian/Packages.gz
}

mkdir -p /artifacts/debian

build_ptp
build_pps_gmtimer
build_openocd
build_firmware
build_kernel_module
build_python_package
build_meta_package
make_repository