#!/bin/bash
#
# Build the project on Debian Slink. This script should be run in Debian Slink container
# created by other scripts after installing all required dependencies.
set -e

ROOT_DIR="/mount"

cd "$ROOT_DIR/gcc-2.8.1_psyq-4.4/gcc"
autoconf && autoheader

cd "$ROOT_DIR"
mkdir -p build/psyq-4.4_old-school
cd build/psyq-4.4_old-school

mkdir -p gcc-2.8.1
cd gcc-2.8.1
"$ROOT_DIR/gcc-2.8.1_psyq-4.4/gcc/configure" --target=mips-psx --host=i686-pc-linux
make -j cc1 CFLAGS="-O2 -g"

cd "$ROOT_DIR/build/psyq-4.4_old-school"
ln -f -s gcc-2.8.1/cc1 cc1psx
