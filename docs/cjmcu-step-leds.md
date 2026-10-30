# Step LEDs on the existing CJMCU-2317 modules

Use the **same two MCP23017 modules as the buttons**, at addresses `0x20` and
`0x21`. Each module handles eight buttons and eight LEDs. No third module is
needed. This wiring is for **ordinary single-colour, two-legged LEDs**; RGB,
addressable LEDs and lamps need a different circuit.

## Wire one LED first

Shut down the Pi and disconnect power before wiring. Keep the modules powered
at **3.3 V**, with common ground and the existing SDA/SCL/RESET/address wiring
from the [button guide](cjmcu-step-buttons.md). Do not change any button wires.

Each LED needs **its own 1 kΩ series resistor**. Start with a red LED, which
usually gives a visible indication at the small current available here.
Blue/white LEDs may be dim at 3.3 V. Do not substitute high-current LEDs or
reduce resistance without checking their specifications and the chip's total
current budget.

```text
Pi 3.3 V rail ── 1 kΩ resistor ── LED anode (+) ── LED cathode (−) ── MCP output
                                      long leg       short leg / flat edge
```

The output sinks current: LOW turns the LED on, HIGH turns it off. The cathode
connects to the MCP pin, not directly to ground. Resistor position in the
series path is interchangeable. Identify polarity from the LED datasheet if
the legs have been trimmed. Never connect an LED without its series resistor.

With a typical red LED at around 2 V, current is approximately
`(3.3 − 2) / 1000 = 1.3 mA`, slightly less allowing for the output voltage drop.
Even a shorted LED is limited to 3.3 mA by its resistor. Eight such branches
per module stay well below the MCP23017's package current limits. Those limits
are absolute maximum ratings, not recommended operating currents.

## Output mapping

Use the module's **GPIO labels**, sometimes printed PA/PB. Address input A0
is different from GPIO GPA0. The button pins remain GPA0–GPA6 and GPB0.

| MCP output | LED on board 0x20 | LED on board 0x21 |
| --- | ---: | ---: |
| GPA7 / PA7 | Step 1 | Step 9 |
| GPB1 / PB1 | Step 2 | Step 10 |
| GPB2 / PB2 | Step 3 | Step 11 |
| GPB3 / PB3 | Step 4 | Step 12 |
| GPB4 / PB4 | Step 5 | Step 13 |
| GPB5 / PB5 | Step 6 | Step 14 |
| GPB6 / PB6 | Step 7 | Step 15 |
| GPB7 / PB7 | Step 8 | Step 16 |

GPA7 and GPB7 are suitable LED **outputs**. The current datasheet restricts
using them as inputs, which is why the button mapping avoids them.

You can test with only board `0x20`: steps 1–8 and their LEDs work normally.
The service logs the missing board `0x21` and retries once per second. All
sixteen buttons and LEDs require both boards.

## Deploy and test

Ansible now enables LEDs through `drum_machine_step_leds_enabled: true` in
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
- [Existing Pi and button wiring](cjmcu-step-buttons.md)
