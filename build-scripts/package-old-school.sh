#!/bin/bash
#
# Package all built artifacts from the "old school" (Debian Slink) build.
set -e

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && cd .. && pwd)
cd "$ROOT_DIR/build"

VERSION=$(cat "$ROOT_DIR/VERSION" | cut -d' ' -f3 | tr -d '"')

tar --transform s/psyq-4.4_old-school/psyq-4.4/ -hcvf "homebrew-psyq-${VERSION}_i386_old-school.tar.gz" psyq-4.4_old-school/cc1psx
