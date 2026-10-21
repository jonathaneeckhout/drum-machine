# drum-machine

A small, standalone drum machine for Linux. Four sample instruments, one editable
16-step pattern, and a C++ engine designed to carry over to an embedded Linux instrument.

For the Pi 3 ARM64 cross-build, Ansible deployment to `drummachine.lab.be`, and
boot service with attachable terminal controls, see [Raspberry Pi deployment](docs/raspberry-pi.md).
Run `./tools/deploy-rpi3.sh` to build and deploy, or add `--skip-build` to deploy
the existing ARM64 package.

![drum-machine](docs/screenshot.png)

## Build and run

Requires a C++20 compiler, CMake 3.24+, Ninja (or another CMake generator), Git,
and Linux audio/window development libraries. Application integration tests also
need Python 3. For Ubuntu/Debian:

```sh
sudo apt install build-essential cmake ninja-build git pkg-config \
    libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev \
    libxcursor-dev libxi-dev libxfixes-dev libxss-dev libwayland-dev \
    libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/drum-machine
```

The desktop configure downloads SDL **3.2.28** (unless a compatible SDL3 CMake package
is installed) and Dear ImGui **1.92.4**. Dependencies are fetched into the build
directory; subsequent builds work offline. Local source checkouts in `.deps/SDL`
and `.deps/imgui` are also supported. The samples are already included, and are
copied alongside the executable during the build.

## Helper scripts

The scripts resolve the repository location themselves, so they work from any
working directory. Builds use Ninja, enable tests, and default to RelWithDebInfo.

```sh
./tools/build.sh --test                # Desktop build and checks
./tools/build.sh headless --test       # Terminal-only build and checks
./tools/build.sh core --debug --test   # Dependency-free Debug build and checks
./tools/build.sh --jobs 8              # Override the default four build jobs
./tools/clean.sh --dry-run             # Preview cleanup
./tools/clean.sh headless              # Remove build-headless/ only
./tools/clean.sh                       # Remove build/, build-headless/, build-core/
```

`CMAKE_BUILD_PARALLEL_LEVEL` also sets the build job count. Both scripts accept
`--help`. Cleanup preserves source files, assets, `.deps/`, and custom build
directories; it refuses symlinked directories and directories without a CMake
cache pointing to this source tree.

## Play

- Click **Play**, or press **Space**, to start and stop. Playback starts at step 1;
  stopping lets existing drum hits decay.
- Click grid cells to toggle hits. Groups of four steps mark the four beats of a bar.
- Drag the tempo value, or Ctrl+click it to type a value, between 40 and 240 BPM.
- Adjust the level below each instrument; **M** mutes and restores its level.
- Click an instrument name, or press **1–4**, to audition it, even while stopped.
- **Clear** removes every step; **Demo** restores the starter groove.

Patterns and settings currently live in memory and reset when the application exits.
The initial scope excludes MIDI, effects, sample browsing, and pattern storage.

## Verify

```sh
ctest --test-dir build --output-on-failure
./build/drum-machine --render-demo /tmp/drum-machine-demo.wav
./build/drum-machine --smoke-test
```

The instrument tests check sample-accurate timing across irregular buffers, looping,
transport, tempo changes, mixing, previews, and the shared control commands. The
terminal command tests exercise parsing, status, input validation, and audible
previews through the same instrument interface. Integration checks run the desktop
with dummy drivers and run the actual terminal executable with no display variables.
The render check loads the samples and exports two bars of the default groove.

For a screenshot, `--screenshot /tmp/drum-machine.bmp` captures the second frame;
combine it with `--smoke-test` for a headless capture. `--assets DIR` overrides the
asset root.

## Terminal frontend

The normal build also supports controls without a graphical window:

```sh
./build/drum-machine --terminal
```

Enter one command per line:

```text
clear
step 1 1
step 1 5
step 1 9
step 1 13
tempo 120
volume 1 0.75
play
status
mute 1
preview 2
stop
quit
```

Track numbers are 1=kick, 2=snare, 3=hi-hat, 4=clap. Steps are 1-16; volume is
0-1; tempo is 40-240 BPM. `step` toggles a hit. `demo` restores the original
pattern. `help` lists commands. Audio runs independently while input waits for the
next command. `quit` or end-of-input stops playback and exits. Nothing is saved.

To omit the desktop frontend and its dependencies entirely:

```sh
cmake -S . -B build-headless -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DDRUM_MACHINE_BUILD_DESKTOP=OFF
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
./build-headless/drum-machine
```

This build defaults to terminal controls, does not fetch/build ImGui, and disables
video, rendering, and GPU support in the fetched SDL build. It needs no graphical
session. If SDL is supplied as a system package, that package's own backend
configuration is retained. Audio still needs a working Linux audio backend/device.
For scripted checks without audio hardware, set `SDL_AUDIODRIVER=dummy`.

The instrument and terminal command tests can also be built without **any** SDL or
ImGui dependency:

```sh
cmake -S . -B build-core -G Ninja -DDRUM_MACHINE_BUILD_APP=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## Architecture and embedded port

```text
Desktop frontend ---\
                      >--- Instrument --- SDL audio output --- Linux audio
Terminal frontend --/          ^
                               |
                        Loaded sample data
```

| Module | Responsibility |
| --- | --- |
| `src/instrument.*` | Instrument commands, status, sequencing, sample playback and mixing. Standard C++ only. |
| `src/audio/sdl_audio.*` | SDL audio lifecycle, output stream and callback. No window or ImGui initialization. |
| `src/audio/samples.*` | Asset discovery, WAV loading and conversion before playback starts. |
| `src/frontends/desktop.*` | Graphical controls, drawing and window lifecycle. |
| `src/frontends/terminal.*` | Line-based controls and status output. Standard C++ only. |
| `src/app/` | Startup option parsing and offline WAV export. |
| `src/main.cpp` | Constructs the modules and selects a frontend. |

Both frontends call the same `Instrument` interface: `play()`, `stop()`,
`togglePlayback()`, `toggleStep()`, `setTempo()`, `setVolume()`, `toggleMute()`,
`clearPattern()`, `loadDemo()`, and `preview()`. They read `status()` to update their
displays. They never touch atomics, pattern masks, voice positions, or sample buffers.
Mute restoration and demo patterns live in the instrument, so physical controls
will get the same behavior as desktop controls.

Commands/status run on one serialized control thread. The audio adapter alone calls
`render()` on a separate audio thread. The instrument privately communicates changes
through lock-free atomics. Status is a value copy of independent observations, not a
transactional snapshot. Core indices are zero-based; invalid indices are ignored.
The terminal translates its human-facing one-based indices into this interface.

The audio adapter requests stereo float PCM at 48 kHz. The instrument schedules
steps on rendered frames, retains fractional timing across buffers and tempo
changes, smooths gains, and applies soft saturation. Its render path performs no
allocations, file operations, or blocking locks on the supported x86-64/AArch64
targets. The adapter owns a fixed scratch buffer; SDL handles hardware conversion.
Each instrument has one playback voice; retriggering restarts that sample. Stopping
halts sequencing while existing hits decay. The audio adapter must be destroyed
before the instrument; the application constructs them in that order.

For physical buttons, encoders, and a display, replace the frontend with a hardware
adapter using the same interface. For example, after decoding/debouncing an encoder:

```cpp
instrument.setTempo(newTempo);
instrument.toggleStep(selectedTrack, pressedStep);
instrument.toggleMute(selectedTrack);
auto displayState = instrument.status();
```

These calls belong on the control thread, never in an interrupt handler or the audio
callback. Serialize input from multiple sources there. GPIO, I2C, and display work
stay outside audio rendering. Keep the SDL audio adapter on Linux, or supply another
adapter calling `render()` if the target requires it. Sample loading is independently
replaceable by constructing the instrument with prepared PCM samples.

Build natively on the board or use a CMake toolchain file for ARM64. A board port has
not yet been verified. Validate display/audio drivers on the target before designing
the enclosure. A 256-frame audio buffer is requested as a hint; actual latency depends
on the backend and device.

Installation is optional:

```sh
cmake --install build --prefix "$HOME/.local"
```

Fetched SDL is linked statically. If you use an installed SDL3 package instead,
ensure its runtime library is also available on the destination.

## Samples and licenses

The four bundled WAV one-shots are original synthesized sounds. Reproduce them with
`python3 tools/generate_samples.py` (Python standard library only, deterministic seed).
There are no external sample-pack dependencies.

Project code and bundled samples use the BSD 3-Clause license; see [LICENSE](LICENSE).
SDL uses the zlib license; Dear ImGui uses the MIT license. Their fetched source
trees retain the upstream notices. See [THIRD_PARTY.md](THIRD_PARTY.md).
