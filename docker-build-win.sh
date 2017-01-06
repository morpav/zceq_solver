#!/bin/bash
set -e

IMAGETAG=slushpool/zceq-solver-buildenv

docker run -t -v $(pwd):/src $IMAGETAG /bin/sh -c "\
    cd /src &&
    scons -c --enable-win-cross-build &&
    virtualenv /venv3 --python=/usr/bin/python3 &&
    /venv3/bin/python ./setup.py build --scons-opts=--enable-win-cross-build bdist_wheel --plat-name='win_amd64'
    chown -R --reference=. build-final dist
"
