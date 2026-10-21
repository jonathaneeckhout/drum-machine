#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/deploy-rpi3.sh [options] [-- additional-ansible-options]

Cross-build and deploy to jojo@drummachine.lab.be using the project inventory.
Creates a project-local Ansible virtual environment automatically when needed.
The deployment installs/enables the boot service and restarts it on updates.

  --skip-build         Deploy the existing build-rpi3 archive without Docker
  --jobs N, -j N       Cross-build jobs (default: CMAKE_BUILD_PARALLEL_LEVEL or 4)
  --syntax-check       Validate Ansible only; do not build or contact the Pi
  --ask-become-pass    Ask for the Pi user's sudo password
  --help, -h           Show this help

Examples:
  tools/deploy-rpi3.sh
  tools/deploy-rpi3.sh --skip-build
  tools/deploy-rpi3.sh --ask-become-pass -- --diff
EOF
}

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
skip_build=false
syntax_only=false
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-4}
ansible_options=()
while (($#)); do
    case "$1" in
        --skip-build) skip_build=true ;;
        --syntax-check) syntax_only=true ;;
        --ask-become-pass) ansible_options+=(--ask-become-pass) ;;
        --jobs|-j)
            if (($# < 2)); then printf '%s requires a positive integer.\n' "$1" >&2; exit 2; fi
            jobs=$2
            shift
            ;;
        --help|-h) usage; exit 0 ;;
        --) shift; ansible_options+=("$@"); break ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done
if [[ ! $jobs =~ ^[1-9][0-9]*$ ]]; then
    printf 'Build jobs must be a positive integer: %s\n' "$jobs" >&2
    exit 2
fi

artifact="$root/build-rpi3/drum-machine-rpi3.tar.gz"
venv="$root/build-tools/ansible"
requirements="$root/deploy/ansible/requirements.txt"

if ! "$syntax_only"; then
    command -v ssh >/dev/null || { printf 'OpenSSH client is required.\n' >&2; exit 1; }
    if "$skip_build"; then
        if [[ ! -s $artifact ]]; then
            printf 'No deployment archive found at %s. Run without --skip-build first.\n' "$artifact" >&2
            exit 1
        fi
    else
        command -v docker >/dev/null || { printf 'Docker is required; use --skip-build with an existing archive.\n' >&2; exit 1; }
        docker info >/dev/null
    fi
fi

command -v python3 >/dev/null || { printf 'Python 3 with venv support is required.\n' >&2; exit 1; }
if [[ -L $root/build-tools || -L $venv ]]; then
    printf 'Refusing a symbolic link for the local Ansible environment.\n' >&2
    exit 1
fi
if [[ ! -x $venv/bin/python || ! -x $venv/bin/ansible-playbook ]]; then
    printf 'Preparing project-local Ansible environment...\n'
    python3 -m venv "$venv"
fi
if [[ ! -x $venv/bin/ansible-playbook ]] || ! cmp -s "$requirements" "$venv/.requirements-installed"; then
    "$venv/bin/python" -m pip install -r "$requirements"
    cp -- "$requirements" "$venv/.requirements-installed"
fi

# Keep Ansible's controller-side temporary files in the ignored build directory.
export ANSIBLE_LOCAL_TEMP="$venv/tmp"
mkdir -p "$ANSIBLE_LOCAL_TEMP"
cd -- "$root"
playbook=("$venv/bin/ansible-playbook" -i "$root/deploy/ansible/inventory.yml" "$root/deploy/ansible/deploy.yml")
"${playbook[@]}" --syntax-check "${ansible_options[@]}"
if "$syntax_only"; then exit 0; fi

if ! "$skip_build"; then bash "$root/tools/build-rpi3.sh" "$jobs"; fi
printf '\nDeploying %s using deploy/ansible/inventory.yml\n' "$artifact"
exec "${playbook[@]}" "${ansible_options[@]}"
