#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/test.sh [headless|desktop|core] [options]

Configure, build, and run tests from any working directory.
Defaults to headless, including audio/service checks without a GUI.

  --debug         Build Debug instead of RelWithDebInfo
  --jobs N, -j N  Parallel build jobs (default: CMAKE_BUILD_PARALLEL_LEVEL or 4)
  --help, -h      Show this help

Use desktop for all desktop checks, or core for tests without SDL.
The script exits nonzero if configuration, compilation, or any test fails.
EOF
}

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
mode=headless
case "${1:-}" in
    headless|desktop|core) mode=$1; shift ;;
esac
for arg in "$@"; do
    case "$arg" in
        --help|-h) usage; exit 0 ;;
    esac
done

exec "$root/tools/build.sh" "$mode" --test "$@"
