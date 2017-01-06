#!/bin/bash
set -e

IMAGETAG=slushpool/zceq-solver-buildenv

#!/bin/bash

# Uncomment this line if compatibility version for core2 is requested (no
# AVX support)
# SCONS_BUILD_OPTS="--march=core2"
docker run -t -v $(pwd):/src $IMAGETAG /bin/sh -c "\
    cd /src &&
    scons -c && scons -c --enable-win-cross-build &&
    scons $SCONS_BUILD_OPTS &&
    virtualenv /venv2 &&
    /venv2/bin/python ./setup.py build --no-scons-build bdist_wheel
    virtualenv /venv3 --python=/usr/bin/python3 &&
    /venv3/bin/python ./setup.py build --no-scons-build bdist_wheel
    chown -R --reference=. build-native build-profiling dist
"
