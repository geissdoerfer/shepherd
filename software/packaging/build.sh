#!/bin/bash
set -e

build_python_package() {
    cd /code/packaging/python3-shepherd

    ln -s /code/python-package python-package

    cd python-package

    python3 setup.py --command-packages=stdeb.command sdist_dsc

    cp /code/packaging/python3-shepherd/debian/rules deb_dist/shepherd-1.0/debian
    cp /code/packaging/python3-shepherd/debian/postinst deb_dist/shepherd-1.0/debian

    cd deb_dist/shepherd-1.0

    dpkg-buildpackage -uc -us

    cp ../python3-shepherd_1.0-1_all.deb /artifacts/debian
}

build_firmware() {
    cd /code/packaging/shepherd-firmware

    ln -s /code/firmware firmware
    ln -s /code/device-tree device-tree

    dpkg-buildpackage -uc -us

    cp ../shepherd-firmware_1-0_all.deb /artifacts/debian
}

build_kernel_module() {
    cd /code/packaging/shepherd-kernel-dkms

    ln -s /code/kernel-module src

    dpkg-buildpackage -uc -us

    cp ../shepherd-kernel-dkms_1.0-0_all.deb /artifacts/debian
}

make_repository() {
    cd /artifacts
    dpkg-scanpackages debian /dev/null | gzip -9c > debian/Packages.gz
}

mkdir -p /artifacts/debian

build_python_package
build_firmware
build_kernel_module
make_repository