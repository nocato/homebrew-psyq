#!/bin/bash
#
# Install all dependencies required to build the project.
# This script should be run in Debian Slink container created by
# other scripts.
set -e

apt-get update
apt-get install -y gcc autoconf libc6-dev make bison gperf
