#!/usr/bin/env bash

CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "Cleaning jsmn..."
cd $CURDIR/libbotty/jsmn && make clean

echo "Cleaning libbotty..."
cd $CURDIR/libbotty && make clean

echo "Cleaning samplebot..."
cd $CURDIR && make clean
