#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/clean.sh [desktop|headless|core|all] [--dry-run]

Remove generated build directories. The default mode is all.

  desktop    Remove build/
  headless   Remove build-headless/
  core       Remove build-core/
  all        Remove all three directories
  --dry-run  Show what would be removed
  --help     Show this help

Source files, assets, .deps/, and other build directories are preserved.
Run this script from any working directory.
EOF
}

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
mode=all
mode_set=false
dry_run=false

while (($#)); do
    case "$1" in
        desktop|headless|core|all)
            if "$mode_set"; then
                printf 'Choose only one cleanup mode.\n' >&2
                exit 2
            fi
            mode=$1
            mode_set=true
            ;;
        --dry-run) dry_run=true ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

case "$mode" in
    desktop) directories=(build) ;;
    headless) directories=(build-headless) ;;
    core) directories=(build-core) ;;
    all) directories=(build build-headless build-core) ;;
esac

# Validate every selected path before deleting anything. Never follow symlinks
# or remove a directory whose CMake cache belongs to another source tree.
for name in "${directories[@]}"; do
    path="$root/$name"
    if [[ -L $path || (-e $path && ! -d $path) ]]; then
        printf 'Refusing to remove a symlink or non-directory: %s\n' "$path" >&2
        exit 1
    fi
    if [[ -d $path ]]; then
        cache="$path/CMakeCache.txt"
        if [[ ! -f $cache ]] || ! grep -Fxq -- "CMAKE_HOME_DIRECTORY:INTERNAL=$root" "$cache"; then
            printf 'Refusing to remove a directory without this project’s CMake cache: %s\n' "$path" >&2
            exit 1
        fi
    fi
done

for name in "${directories[@]}"; do
    path="$root/$name"
    [[ -d $path ]] || continue
    if "$dry_run"; then
        printf 'Would remove %s\n' "$path"
    else
        printf 'Removing %s\n' "$path"
        rm -rf -- "$path"
    fi
done
