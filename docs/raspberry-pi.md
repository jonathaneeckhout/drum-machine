# Raspberry Pi 3 build and deployment

Target: Raspberry Pi 3 Model B running **64-bit Debian 13 (trixie)**, reachable
as `jojo@drummachine.lab.be`. This is the OS observed on the target, not a
generic package for every Raspberry Pi OS version. No desktop or graphical
session is required. The current software still has four sample tracks; the
eight-track hardware plan is separate work.

## Build on the development computer

Install Docker using its normal platform instructions. From the repository:

```sh
bash tools/build.sh headless --test
bash tools/build-rpi3.sh
```

The second command builds an amd64/host Docker environment with a Debian ARM64
cross compiler, then compiles for the Pi's Cortex-A53. SDL is built as a static
library with ALSA and dummy audio, without the desktop GUI, PulseAudio,
PipeWire or JACK. System libraries including glibc, libstdc++ and ALSA remain
dynamic. No ARM emulation is needed to compile.

The Docker base image is pinned by digest. Debian package repositories still
receive updates, so this is a repeatable build procedure, not a guarantee of
byte-identical output across future package updates. Check target compatibility
again if changing the image or Pi OS.

Outputs:

- `build-rpi3/package/bin/drum-machine`
- `build-rpi3/package/share/drum-machine/` (samples and license notices)
- `build-rpi3/drum-machine-rpi3.tar.gz` (deployment artifact)

The optional positional argument controls parallelism: `bash tools/build-rpi3.sh 8`.
Subsequent builds reuse the Docker image and build directory. Run target binaries
on ARM64; do not run ordinary CTest against the cross-build on an x86 host.

The explicit CMake option is `DRUM_MACHINE_RPI3=ON`. It selects Cortex-A53
optimisation, defaults the desktop off and rejects incompatible targets. It
does not install a cross compiler by itself. In a compatible Debian multiarch
development environment the equivalent configure is:

```sh
cmake -S . -B build-rpi3 -G Ninja \
  --toolchain cmake/toolchains/rpi3-aarch64.cmake \
  -DDRUM_MACHINE_RPI3=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-rpi3 --parallel 4
```

On the ARM64 Pi itself, omit the toolchain file and configure with
`-DDRUM_MACHINE_RPI3=ON`; that is a native build and needs CMake, Ninja, GCC,
Git, pkg-config and ALSA development headers. Use a fresh build directory when
switching compilers or architectures.

## Deploy using Ansible

Install Ansible on the development computer, not the Pi. A local virtual
environment keeps it isolated:

```sh
python3 -m venv build-tools/ansible
build-tools/ansible/bin/pip install -r deploy/ansible/requirements.txt
build-tools/ansible/bin/ansible-playbook \
  -i deploy/ansible/inventory.yml deploy/ansible/deploy.yml --syntax-check
```

Check that normal SSH works first, including trusted host-key verification:

```sh
ssh jojo@drummachine.lab.be
```

Then deploy from the development computer:

```sh
build-tools/ansible/bin/ansible-playbook \
  -i deploy/ansible/inventory.yml deploy/ansible/deploy.yml
```

Use `--ask-become-pass` if sudo needs a password. No credentials or SSH keys are
stored in the inventory, and SSH host-key checking is not disabled.

The playbook checks Debian/ARM64 compatibility, installs runtime dependencies,
extracts a checksum-named release under `/opt/drum-machine/releases/`, and
renders a demo WAV before selecting that release. This checks executable/library
compatibility and sample loading without playing audio. It installs and enables
`drum-machine.service`, starts the engine as `jojo`, and checks the local control
socket with `status`. It does not build SDL or install a compiler on the Pi.
Updates restart the engine and reset the in-memory pattern. Old releases remain
on disk; inspect and prune unused releases manually when necessary.

The default output is the Pi's onboard headphone jack. Override
`drum_machine_audio_device` in `deploy/ansible/group_vars/drum_machines.yml`
when using USB audio or HDMI; find device names with `aplay -L` on the Pi.

The controller requires Python 3 with venv/pip support; the Pi needs SSH,
Python 3, sudo and systemd. Deployment changes runtime packages, `/opt`, the
system service and the terminal launcher. Merely building or syntax-checking
does not deploy anything. Full check mode on a first installation cannot verify
uninstalled binaries or a not-yet-extracted release; use syntax-check and review
the playbook before the first real deployment.

## Terminal control after deployment

```sh
ssh jojo@drummachine.lab.be
drum-machine
```

Enter commands such as:

```text
status
play
tempo 128
volume 1 0.6
mute 2
stop
quit
```

`quit`, EOF or closing SSH only disconnects the client. The engine and audio
continue running. `stop` stops playback. The service starts in the stopped
state; it does not start a beat automatically. To send a single command:

```sh
drum-machine --command status
drum-machine --command play
```

The `/usr/local/bin/drum-machine` launcher always attaches to the service.
It invokes the installed executable with `--connect`. Standalone operation is
still available by calling the actual executable with `--terminal` explicitly,
but stop the service first if it owns the audio device.

Only the service user (and root) can access `/run/drum-machine/control.sock`.
No network port is opened. Requests are serialized on the control thread;
they never run on the audio callback thread. Other local development setups
can select a socket with `--socket PATH` or `DRUM_MACHINE_SOCKET`.

For manual service testing, create a private runtime directory and use:

```sh
mkdir -p /tmp/drum-machine-test
chmod 700 /tmp/drum-machine-test
SDL_AUDIODRIVER=dummy ./build-headless/drum-machine --serve \
  --socket /tmp/drum-machine-test/control.sock
# From another terminal:
./build-headless/drum-machine --connect --socket /tmp/drum-machine-test/control.sock
```

Use a directory owned by your user. The server refuses to replace an existing
socket. After an unclean exit in a manual setup, confirm no engine is running
before removing its stale socket. systemd manages and cleans the production
runtime directory automatically.

Service operations on the Pi:

```sh
systemctl status drum-machine
sudo journalctl -u drum-machine -f
sudo systemctl restart drum-machine
sudo systemctl stop drum-machine
```

Pattern/settings persistence and physical GPIO/ADC/LED support are not added
by this deployment work. Patterns currently reset when the engine restarts.
