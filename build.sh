#!/bin/sh
premake5 gmake2
pushd .build/gmake2
make
popd
