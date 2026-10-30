# Hardware design and implementation status

The current prototype uses a Raspberry Pi 3 with the purchased MCP23017 /
CJMCU-2317 modules. Follow the [combined wiring schema](hardware-wiring.md)
for all Pi, button and LED connections, and the [parts list](hardware-parts-list.md)
for the components needed now.

## Current prototype

- Four sample tracks: kick, snare, hi-hat and clap; 16 steps per track.
- Sixteen reused step buttons, with one single-colour LED above each.
- Two MCP23017 modules at `0x20` and `0x21`, each handling eight buttons and
  eight LEDs. No separate LED driver is required.
- SSH terminal controls for track selection, transport, tempo, volume and mute.
- Pi 3 onboard headphone output is the deployment default.
- A self-built MDF enclosure is planned at approximately 40 × 25 × 5 cm,
  with a hinged lid. Confirm usable internal space against the actual controls,
  module standoffs and cable bends before cutting the final panels.

Selecting a track through `select 1`–`select 4` makes the physical step buttons
edit that track. LEDs show its programmed hits. During playback the current
step's LED inverts, making the cursor visible on both filled and empty steps.
Stopping restores the pattern display and lets existing drum hits decay.

The Pi service keeps running when SSH disconnects. Patterns and settings live
in memory and reset when the service restarts; no save or shutdown button is
implemented. This prototype does not include physical sliders, track selection,
mute or transport buttons, an encoder or a display.

## Software architecture

Button debounce and LED updates run on the same serialized control thread as
terminal commands. I²C operations stay outside the audio callback. Missing
modules recover independently without terminating the audio/CLI service.
The driver configures all pins on both modules exclusively.

Use [the test helper](../tools/test.sh) for host checks and the
[Pi build/deployment guide](raspberry-pi.md) for the ARM64 service. Electrical
assembly and end-to-end latency still need checking on the physical instrument.

## Future panel

The revised target is eight tracks with select/mute buttons, selection LEDs,
volume sliders and sequence navigation. Its control counts and unresolved
implementation work are in the [eight-track plan](hardware-wiring-8-tracks.md).
Earlier four-track proposals for extra mute LEDs, an OLED, tempo encoder,
Clear/Shutdown buttons and automatic saving are not part of the current build.
Decide whether to retain those extras before allocating more pins or parts.

Keep the 16 steps in four visual groups of four. Use a cardboard panel mockup
to check button spacing and slider travel. Mount modules on suitable standoffs
and leave access for the Pi's SD card, power/audio connectors, ventilation and
lid wiring. Exact mounting hardware depends on the actual board holes and panels.
