#!/usr/bin/env sh
set -eu

mkdir -p build

gcc main.c lexerf.c parserf.c semanticf.c codegeneratorf.c hashmap/hashmapoperators.c -o build/unn -Wall -Wextra
