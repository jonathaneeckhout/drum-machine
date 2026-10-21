#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-4}
if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    cat <<'EOF'
Usage: tools/build-rpi3.sh [parallel-jobs]

Cross-compile on this computer in a Debian 13 Docker container for Pi 3 ARM64.
Requires Docker. First run downloads the image, compiler and SDL.
Output: build-rpi3/package/ (bin/, share/), build-rpi3/drum-machine-rpi3.tar.gz
Run host tests separately with tools/build.sh headless --test.
EOF
    exit 0
fi
if (($# > 1)); then printf 'Expected at most one job count.\n' >&2; exit 2; fi
jobs=${1:-$jobs}
if [[ ! $jobs =~ ^[1-9][0-9]*$ ]]; then printf 'Job count must be positive.\n' >&2; exit 2; fi
if [[ -L $root/build-rpi3 ]]; then printf 'Refusing symlinked build-rpi3 directory.\n' >&2; exit 1; fi
command -v docker >/dev/null || { printf 'Docker is required for the Pi cross-build.\n' >&2; exit 1; }
mkdir -p "$root/build-rpi3"
docker build --tag drum-machine-cross:debian13 "$root/deploy/cross"
docker run --rm --user "$(id -u):$(id -g)" \
    --env HOME=/tmp/drum-cross-home \
    --volume "$root:/work" --workdir /work drum-machine-cross:debian13 \
    bash -euc '
        cmake -S . -B build-rpi3 -G Ninja \
            --toolchain cmake/toolchains/rpi3-aarch64.cmake \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/drum-machine \
            -DDRUM_MACHINE_RPI3=ON -DDRUM_MACHINE_BUILD_DESKTOP=OFF -DBUILD_TESTING=ON
        cmake --build build-rpi3 --parallel "$1"
        cmake --install build-rpi3 --prefix /work/build-rpi3/package
        aarch64-linux-gnu-readelf -h build-rpi3/package/bin/drum-machine
        tar -C build-rpi3/package -czf build-rpi3/drum-machine-rpi3.tar.gz bin share
    ' cross-build "$jobs"
printf '\nPi artifact: %s\n' "$root/build-rpi3/drum-machine-rpi3.tar.gz"
