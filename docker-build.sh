#!/bin/bash
set -e

IMAGETAG=slushpool/zceq-solver-buildenv

docker run -t -v $(pwd):/src $IMAGETAG /bin/sh -c "\
    cd /src &&
    scons &&
    chown -R --reference=. build-final build-profiling/
"

