# Homebrew Psy-Q
*Linux build of GCC (CC1PSX.EXE) from PsyQ 4.4*

## About

Homebrew Psy-Q is a project to compile a Linux build of GCC (CC1PSX.EXE) from [PsyQ](https://www.retroreversing.com/official-playstation-1-software-development-kit-(psyq)/) 4.4. The resulting compiler executable can be run on a modern Linux system, producing the same exact machine code as CC1PSX.EXE from PsyQ 4.4. The compatibility was tested on the [mgs_reversing](https://github.com/FoxdieTeam/mgs_reversing) project, replacing CC1PSX.EXE and producing the same exact binary.

The GCC code was imported from the [GCC git repository](https://github.com/gcc-mirror/gcc) and minimal set of patches from the source tarball and PsyQ 4.4 GCC fork were applied.

## To do
- Although the compiler executable correctly runs on a modern Linux system, the build process of this executable requires an old Linux system. See [Building](#building).
- Fix build flags: the compiler should be a 64-bit, `-O2`/`-O3` optimized binary.
- Automated CI build.
- Add ability to generate files with Windows file endings. The assembler from PsyQ requires this (a workaround is to run unix2dos on the resulting file).
- PsyQ 4.3, 4.0 (or other?).
- Some kind of replacement for PsyQ's assembler? (their assembler is not based on GNU as)

## Building
A pre-built binary can be downloaded from the [Releases tab](https://github.com/nocato/homebrew-psyq/releases).

Currently the build process requires an old Linux system.

1. Install Ubuntu 6.04 in a VM. Use [this ISO file](http://old-releases.ubuntu.com/releases/warty/warty-release-install-i386.iso) (sha1: `7b56525113d6a020d63785c44e602d802debef8e`). If you are using VirtualBox make sure to configure your virtual disk as a IDE device (not SATA).
2. `sudo apt-get install gcc g++`
3. Install autoconf:
    1. `wget http://ftp.gnu.org/gnu/autoconf/autoconf-2.13.tar.gz` (sha1: `e4826c8bd85325067818f19b2b2ad2b625da66fc`)
    2. `gunzip autoconf-2.13.tar.gz && tar -xf autoconf-2.13.tar && cd autoconf-2.13`
    3. `./configure && make && sudo make install`
    4. `cd ..`
4. Install Bison:
    1. `wget http://ftp.gnu.org/gnu/bison/bison-1.27.tar.gz` (sha1: `f9b8b3fc2cb7348e172ee5284d3bb71468ba5324`)
    2. `gunzip bison-1.27.tar.gz && tar -xf bison-1.27.tar && cd bison-1.27`
    3. `./configure && make && sudo make install`
    4. `cd ..`
5. Install gperf:
    1. `wget http://ftp.gnu.org/gnu/gperf/gperf-2.7.2.tar.gz` (sha1: `a8a096093d9f94650bac1e90c8c8bccf49529444`)
    2. `gunzip gperf-2.7.2.tar.gz && tar -xf gperf-2.7.2.tar && cd gperf-2.7.2`
    3. `./configure && make && sudo make install`
    4. `cd ..`
6. `cd homebrew-psyq/gcc-2.8.1_psyq-4.4/gcc/`
7. `autoconf && autoheader`
8. `./configure --target=mips-psx --build=i686-pc-linux`
9. `make cc1`
10. The compiler executable `cc1` should be in your current directory.
