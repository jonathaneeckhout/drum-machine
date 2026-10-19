# Hardware design

> Historical four-track design. The eight-track control list and connection
> proposal from 18 October 2026 are in [hardware-wiring-8-tracks.md](hardware-wiring-8-tracks.md).

Design agreed on 17 October 2026. This records the first prototype's controls
and behaviour. Component selection, pin assignments and enclosure dimensions
remain open until the existing parts have been inventoried.

## Panel

- One row of 16 reused step buttons, with one LED above each button.
- Visually group the steps into four groups of four.
- Four mixer strips: kick, snare, hi-hat and clap.
- Each strip has a volume slider, track-select button and selection indicator,
  plus a mute button and mute indicator.
- One small display for BPM, selected track and save/shutdown status.
- One tempo rotary encoder with push action for fine adjustment.
- Separate Play and Stop buttons, and a Clear button.
- A shutdown button positioned away from the performance controls.
- Stereo line output and wall power.

Reuse the owner's existing buttons. Determine panel spacing with a cardboard
mockup before committing to an enclosure.

## Interaction

Selecting a track makes the step buttons edit that track. Its programmed hits
appear on the step LEDs. The current playback step must remain distinguishable
from a programmed hit; test a brightness/pulse scheme with the actual LEDs.

Each slider always controls its own track, regardless of track selection. Mute
preserves the track's selected volume. Slider changes while muted update the
volume that will be heard when the track is unmuted.

Play starts from step 1. Stop returns the sequencer to step 1 without erasing the
beat. Existing drum hits may finish decaying. Hold Clear and press a track-select
button to erase that track.

Turn the encoder to change tempo; its push action provides finer adjustment.
Exact increments and whether the push action is held or toggled need a hands-on
check with the chosen encoder.

The first version stores one beat and restores it after a normal shutdown.
Shutdown saves the beat and settings before halting the Pi. A shutdown button
does not itself disconnect power; the display should indicate when power can be
removed. Automatic saving, startup behaviour after power restoration and any
power-switch circuitry are implementation decisions still to settle.

## Electronics architecture

```text
Step, track, mute and transport buttons --> Digital input expansion --+
Four volume sliders ---------------------> ADC ----------------------+
Tempo encoder ------------------------------------------------------+--> Pi 3
Small display <-----------------------------------------------------+
Step, selection and mute LEDs <----------- LED driver circuitry <---+
Stereo line output <--------------------- Audio interface <---------+
```

The Pi runs the existing sample-based audio engine. Start with directly connected
peripheral modules; a separate control microcontroller is not required by this
design. Keep audio scheduling independent of panel scanning and display updates.
Pi performance and audio latency must be measured on the actual hardware.

| Signal | Quantity | Connection plan |
| --- | ---: | --- |
| Step buttons | 16 | Digital input expansion |
| Track-select buttons | 4 | Digital input expansion |
| Mute buttons | 4 | Digital input expansion |
| Play, Stop, Clear, Shutdown | 4 | Digital inputs; reserve a wake-capable connection separately if required |
| Encoder A/B and push switch | 3 | Prefer direct GPIO; exact allocation pending |
| Volume sliders | 4 analog | External ADC |
| Step LEDs | 16 | LED drivers |
| Track-select and mute indicators | 8 | LED drivers |
| Display | 1 | Interface depends on selected module |
| Audio | Stereo | Final audio interface and connectors pending |

Allow capacity for at least 28 panel button inputs, three encoder signals and
24 indicator outputs. Expansion design must support Clear plus track-select
being pressed together. Add button debouncing and filtering of slider readings.

Use 3.3 V-compatible logic throughout the Pi interface. Provide appropriate
LED current limiting and size the power supply for the complete assembly.

An MCP3008 is a candidate ADC: eight channels over SPI provide four spare
channels after the volume sliders. It is not a committed purchase.

Sources:

- [Raspberry Pi GPIO documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
- [Microchip MCP3008 specifications](https://www.microchip.com/en-us/product/MCP3008)

## Software work

The current project supports four tracks, 16 steps, per-track volume and mute,
and operation without a graphical desktop. Hardware input, display/LED feedback,
pattern persistence and shutdown integration still need implementing.

## Inventory needed before the wiring plan

- Exact Pi 3 variant and available power supply.
- Sliders: quantity, resistance markings, travel length and pin arrangement.
- LEDs: ordinary two-lead, multicolour or addressable; any known ratings.
- Any existing ADC, digital input expansion, LED driver, display, encoder or
  audio modules, including their model markings.
- Button electrical type (momentary or latching), measured during assembly;
  no need to choose a new button style.

Budget is unspecified. Reuse available parts and cost the remaining components
before committing to a parts list. Do not assign final pins or cut panel holes
until module interfaces and actual component dimensions are known.
