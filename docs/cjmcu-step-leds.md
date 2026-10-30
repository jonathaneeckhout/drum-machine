# CJMCU-2317 step-LED operation and testing

Use **[the combined wiring schema](hardware-wiring.md)** for the Pi, both
modules, LED polarity, resistors and the full step pin table. The same two
MCP23017 modules handle buttons and LEDs; no third board or separate LED
driver is needed. The circuit assumes ordinary single-colour, two-legged LEDs,
with one 1 kΩ resistor per LED.

A single module at `0x20` supports steps 1–8 and their LEDs. The missing second
module is retried independently without terminating the service.

## Deploy and test

Ansible enables LEDs through `drum_machine_step_leds_enabled: true` in
`deploy/ansible/group_vars/drum_machines.yml`. It also requires
`drum_machine_step_buttons_enabled: true`. Set the LED option false to retain
the previous button-only configuration. When enabled, all eight spare pins
on each board are LED outputs; remove other circuits from those pins first.

From the development computer:

```sh
./tools/deploy-rpi3.sh --ask-become-pass
ssh jojo@drummachine.lab.be
drum-machine
```

In the CLI, test the first LED after connecting it to board `0x20` GPA7:

```text
stop
clear
select 1
step 1 1
status
```

Step 1's LED should light. Enter `step 1 1` again to switch it off. `clear`
removes all programmed steps; nothing is saved across service restarts.

LEDs display the **selected track's pattern**. Selecting another track through
`select 2`, for example, changes the displayed pattern. During playback the
current step's LED is inverted: a programmed hit goes dark and an empty step
lights up for that step. Stopping restores the normal pattern display. There
is no brightness adjustment. Button changes appear on the next control-loop
iteration, nominally within 5 ms after debounce.

For manual service startup use `--serve --step-leds`; this also enables the
step buttons. The existing `--i2c-device` and `--step-addresses` options apply
to both. The installed `drum-machine` launcher connects to the systemd service.
Stop that service before running a second executable against the modules.

The driver sets LED latches off before enabling the output pins, writes only
when the display changes, and restores the display after reconnecting a
module. It attempts to turn LEDs off on orderly service exit. An abrupt power
loss, process kill or bus failure cannot guarantee that shutdown write.

## References

- [Microchip MCP23017 datasheet: pin functions, register mapping and electrical limits](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-MCP23S17-16-Bit-IO-Expander-with-Serial-Interface-DS20001952.pdf)
- [Combined Pi, button and LED wiring](hardware-wiring.md)
- [Button operation and diagnostics](cjmcu-step-buttons.md)
