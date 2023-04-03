#!/bin/bash
#
# Unpackage artifacts from the "old school" (Debian Slink) build
# and try to build mgs_reversing with that compiler.
set -e

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && cd .. && pwd)
cd "$ROOT_DIR/build"

rm -rf test

mkdir test
cd test

cp ../homebrew-psyq-*_i386_old-school.tar.gz .
tar -xavf *.tar.gz

git clone https://github.com/FoxdieTeam/psyq_sdk.git
cd psyq_sdk
git checkout dac47a48d0ed6d255b26a5dc104ce319a6bbe963

rm psyq_4.4/bin/CC1PSX.EXE
cp ../psyq-4.4/cc1psx psyq_4.4/bin/CC1PSX.EXE
chmod +x psyq_4.4/bin/CC1PSX.EXE
ln -s ASM.H psyq_4.4/INCLUDE/asm.h
ln -s R3000.H psyq_4.4/INCLUDE/r3000.h
sed -i 's/sys\/fcntl.h/SYS\/FCNTL.H/g' psyq_4.4/INCLUDE/SYS/FILE.H

cd ..

git clone https://github.com/FoxdieTeam/mgs_reversing.git
cd mgs_reversing
git checkout a94e1f6026ea303ce5f76b6eb9e922dcbe226265

cd build
sed -i 's/prefix("wine", "\$psyq_path\/psyq_4.4\/bin\/CC1PSX.EXE")/"\$psyq_path\/psyq_4.4\/bin\/CC1PSX.EXE"/g' build.py

# Too long path names can cause build failures
pwd

pip install -r requirements.txt
python3 build.py
