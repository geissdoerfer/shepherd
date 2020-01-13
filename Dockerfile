FROM ubuntu:bionic
ENV TINI_VERSION v0.18.0

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    debhelper \
    build-essential \
    bison \
    flex \
    pkg-config \
    gcc-arm-linux-gnueabihf \
    libc6-dev-armhf-cross \
    libtool \
    automake \
    autotools-dev \
    libyaml-dev \
    pkg-config \
    git \
    wget \
    libc6-i386 \
    fakeroot \
    dkms \
    devscripts \
    dpkg-dev \
    gzip \
    equivs \
    python-all \
    python3-all \
    python3-setuptools \
    python3-stdeb \
    python3-pytest-runner

WORKDIR /tools/
RUN git clone --depth 1 https://git.kernel.org/pub/scm/utils/dtc/dtc.git
RUN cd dtc && make all
RUN cd dtc && make PREFIX=/usr/local/ CC=gcc CROSS_COMPILE= all
RUN cd dtc && make PREFIX=/usr/local/ install


RUN wget http://software-dl.ti.com/codegen/esd/cgt_public_sw/PRU/2.3.1/ti_cgt_pru_2.3.1_linux_installer_x86.bin
RUN chmod +x ti_cgt_pru_2.3.1_linux_installer_x86.bin
RUN ./ti_cgt_pru_2.3.1_linux_installer_x86.bin --mode unattended --prefix /tools

RUN wget --retry-on-http-error=503,301 http://git.ti.com/cgit/cgit.cgi/pru-software-support-package/pru-software-support-package.git/snapshot/pru-software-support-package-5.4.0.tar.gz
RUN tar -xvf pru-software-support-package-5.4.0.tar.gz

ENV PRU_CGT /tools/ti-cgt-pru_2.3.1
ENV PRU_SUPPORT /tools/pru-software-support-package-5.4.0

ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
RUN chmod +x /tini

COPY build.sh /
RUN chmod +x /build.sh

ENTRYPOINT ["/tini", "--"]

CMD ["/bin/bash"]
