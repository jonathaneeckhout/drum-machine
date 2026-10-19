#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/build.sh [desktop|headless|core] [options]

Configure and build from any working directory. The default mode is desktop.

  --debug         Build Debug instead of RelWithDebInfo
  --test          Run CTest after building
  --jobs N, -j N  Parallel build jobs (default: CMAKE_BUILD_PARALLEL_LEVEL or 4)
  --help, -h      Show this help

Modes use build/, build-headless/, and build-core/ respectively.
The first application build may download SDL and, for desktop, Dear ImGui.
EOF
}

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
mode=desktop
mode_set=false
build_type=RelWithDebInfo
run_tests=false
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-4}

while (($#)); do
    case "$1" in
        desktop|headless|core)
            if "$mode_set"; then
                printf 'Choose only one build mode.\n' >&2
                exit 2
            fi
            mode=$1
            mode_set=true
            ;;
        --debug) build_type=Debug ;;
        --test) run_tests=true ;;
        --jobs|-j)
            if (($# < 2)); then
                printf '%s requires a positive integer.\n' "$1" >&2
                exit 2
            fi
            jobs=$2
            shift
            ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ ! $jobs =~ ^[1-9][0-9]*$ ]]; then
    printf 'Build jobs must be a positive integer: %s\n' "$jobs" >&2
    exit 2
fi

app=ON
desktop=ON
case "$mode" in
    desktop) build_dir="$root/build" ;;
    headless) build_dir="$root/build-headless"; desktop=OFF ;;
    core) build_dir="$root/build-core"; app=OFF; desktop=OFF ;;
esac

if [[ -L $build_dir ]]; then
    printf 'Refusing to build through a symbolic link: %s\n' "$build_dir" >&2
    exit 1
fi

printf 'Building %s (%s) in %s\n' "$mode" "$build_type" "$build_dir"
cmake -S "$root" -B "$build_dir" -G Ninja \
    "-DCMAKE_BUILD_TYPE=$build_type" \
    -DBUILD_TESTING=ON \
    "-DDRUM_MACHINE_BUILD_APP=$app" \
    "-DDRUM_MACHINE_BUILD_DESKTOP=$desktop"
cmake --build "$build_dir" --parallel "$jobs"

if "$run_tests"; then
    ctest --test-dir "$build_dir" --output-on-failure
fi

if [[ $mode != core ]]; then
    printf '\nRun: %q\n' "$build_dir/drum-machine"
fi
