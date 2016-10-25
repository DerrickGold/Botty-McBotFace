#!/usr/bin/env bash

CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ ! -f $CURDIR/libbotty/botty.a ]; then
    cd $CURDIR/libbotty && make
    cd $CURDIR
fi

make
