# docker pull zceq-solver-buildenv

FROM debian:sid

MAINTAINER Ondrej Sika <ondrej@ondrejsika.com>

RUN apt-get update && apt-get install -y \
        gcc-6 \
        g++-6 \
        make \
        cmake \
        llvm-3.9 \
        clang-3.9 \
        mingw-w64 \
        scons \
	virtualenv \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 999 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 999 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.9 999 \
    && update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-3.9 999

RUN apt-get update && apt-get install -y \

