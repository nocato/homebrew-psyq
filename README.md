# Homebrew Psy-Q
*Linux build of GCC (CC1PSX.EXE) from PsyQ 4.4*

## About

Homebrew Psy-Q is a project to compile a Linux build of GCC (CC1PSX.EXE) from [PsyQ](https://www.retroreversing.com/official-playstation-1-software-development-kit-(psyq)/) 4.4 for PlayStation 1 (PSX) development. The resulting compiler executable can be run on a modern Linux system, producing the same exact machine code as CC1PSX.EXE from PsyQ 4.4. The compatibility was tested on the [mgs_reversing](https://github.com/FoxdieTeam/mgs_reversing) project, replacing CC1PSX.EXE and producing the same exact binary.

The GCC code was imported from the [GCC git repository](https://github.com/gcc-mirror/gcc) and a minimal set of patches from the source tarball and PsyQ 4.4 GCC fork was applied.

## To do
- Make it possible to build the project natively on a modern system without a "old school" Debian Docker container (see [Building](#building)).
- Automated CI testing of [mgs_reversing](https://github.com/FoxdieTeam/mgs_reversing) compatiblity.
- Add ability to generate files with Windows file endings. The assembler from PsyQ requires this (a workaround is to run unix2dos on the resulting file).
- Additional debug logging around register allocation, different passes of the compiler (for the purposes of matching decompilation projects, such as [mgs_reversing](https://github.com/FoxdieTeam/mgs_reversing)).
- PsyQ 4.3, 4.0 (or other?).
- Some kind of replacement for PsyQ's assembler? (their assembler is not based on GNU as)
- Windows build?

## Building
A pre-built binary can be downloaded from the [Releases tab](https://github.com/nocato/homebrew-psyq/releases).

Docker or Podman is required for the build process. To build the project run:
```bash
make psyq-4.4-old-school
```

This command builds the project using an "old school" Debian 2.1 Slink (from 1997) Docker image. The resulting `cc1psx` binary is placed into `build/psyq-4.4_old-school` directory.
