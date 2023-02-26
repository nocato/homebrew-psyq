#!/bin/bash
#
# Create a Docker container with Debian Slink, install required dependencies
# on it and run a build.
set -e

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && cd .. && pwd)

DOCKER="docker"

if command -v podman &> /dev/null
then
    DOCKER="podman"
fi

if ! command -v $DOCKER &> /dev/null
then
    echo "Docker or Podman not found on your system. Please install Docker or Podman before running the build." >&2
    exit 1
fi

# slink-slim as of 26-02-2023
IMAGE="docker.io/debian/eol@sha256:50b8edf5ebbc6a6e266cf6972c32ac6ecb293222d82f6f191dc4a7df49f217ba"

$DOCKER run --privileged -i --rm \
    --volume "$ROOT_DIR:/mount:rw,z" \
    "$IMAGE"  \
    sh -c "./mount/build-scripts/install-dependencies-debian-slink.sh && ./mount/build-scripts/build-debian-slink.sh"
