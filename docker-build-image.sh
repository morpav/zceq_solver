#!/bin/bash
set -e

IMAGETAG=slushpool/zceq-solver-buildenv

docker build -t $IMAGETAG .

