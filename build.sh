#!/usr/bin/env sh
set -eu

mkdir -p build

gcc main.c diagnostics.c lexerf.c parserf.c semanticf.c optimizerf.c codegeneratorf.c -o build/bhasacore -Wall -Wextra -std=c11
