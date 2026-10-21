# CJMCU-2317 step-button wiring and operation

This guide implements the purchased [MCP23017 / CJMCU-2317 module pack](https://www.amazon.com.be/dp/B0DGKKR9BV).
Use **two modules for the 16 step buttons**, eight buttons on each. This mapping
supersedes the earlier 12-input-per-board allocation in the eight-track hardware
proposal. The software currently has four tracks; track selection, playback,
tempo and other controls stay available through SSH.

## Pi to both modules

Disconnect power before changing wiring. Power the modules at **3.3 V**, not
5 V, so that their bus signals and any onboard pull-ups stay compatible with
the Pi. All grounds must connect together.

| Pi 3 physical header pin | Pi function | Connect to both CJMCU modules |
| ---: | --- | --- |
| 1 | 3.3 V | VCC / VDD |
| 6 | Ground | GND / VSS |
| 3 | GPIO2 / SDA1 | SDA |
| 5 | GPIO3 / SCL1 | SCL |
| 1 | 3.3 V | RESET / RST (hold high) |

These are **physical header pin numbers**, not BCM GPIO numbers. Leave INTA,
INTB and NC pins unconnected; this implementation polls rather than using
interrupt wires. Module header order varies: use the printed signal names,
not the apparent position of a pin in a product photograph.

```text
Pi 3                         Board 1                  Board 2
3.3 V (physical 1) ----------- VCC + RESET ------------ VCC + RESET
GND   (physical 6) ----------- GND -------------------- GND
SDA   (physical 3 / GPIO2) --- SDA -------------------- SDA
SCL   (physical 5 / GPIO3) --- SCL -------------------- SCL
```

Set the three **address inputs** to fixed levels:

| Module | Address | Address A2 | Address A1 | Address A0 |
| --- | --- | --- | --- | --- |
| Board 1: steps 1–8 | 0x20 | GND | GND | GND |
| Board 2: steps 9–16 | 0x21 | GND | GND | 3.3 V |

Do not leave address inputs floating. **Address A0 is not GPIO GPA0**: the
address pins choose the board, whereas the GPIO port pins connect to buttons.
GPIO labels may be abbreviated PA0/PB0 or port A0/B0 on a module; identify the
address group separately. The seller does not provide a verified board schematic,
so inspect any fitted address links before changing them; do not connect 3.3 V
to a pin already directly strapped to ground.

The Pi already has I²C pull-ups. For short prototype wiring, start without
adding external SDA/SCL pull-ups. Check the module's pull-ups and total bus
loading if extending the wiring or adding more boards. Keep bus wiring short.

## Buttons to modules

Use **momentary normally open** switches. Each switch connects its assigned
GPIO input to GND when pressed. The driver enables the MCP23017's internal
pull-up resistors, so no separate pull-up resistor is needed for each button.

```text
                     inside MCP23017
3.3 V --- internal pull-up --- GPIO input --- button --- GND

Released: input HIGH                  Pressed: input LOW
```

| GPIO function on module | Board 1 button | Board 2 button |
| --- | ---: | ---: |
| GPA0 / PA0 | Step 1 | Step 9 |
| GPA1 / PA1 | Step 2 | Step 10 |
| GPA2 / PA2 | Step 3 | Step 11 |
| GPA3 / PA3 | Step 4 | Step 12 |
| GPA4 / PA4 | Step 5 | Step 13 |
| GPA5 / PA5 | Step 6 | Step 14 |
| GPA6 / PA6 | Step 7 | Step 15 |
| GPB0 / PB0 | Step 8 | Step 16 |

**Do not connect buttons to GPA7 or GPB7.** The current Microchip datasheet
restricts those pins to outputs on the MCP23017. The driver sets them as low
outputs; leave them unconnected. GPB1–GPB6 are spare inputs, currently ignored
by the application.

For a four-legged tactile switch, identify the two internally connected pairs
with a multimeter. Use one terminal from each pair so the connection closes
only while pressing. Two terminals in the same pair are already connected and
would appear as a permanently held button.

## Deploy and enable I²C

From the development computer, build and deploy the updated application:

```sh
./tools/deploy-rpi3.sh
```

Ansible enables step buttons by default for the configured Pi. It installs
I²C tools, configures `dtparam=i2c_arm=on` in the configured firmware file,
loads `i2c-dev`, and gives the service user access through the `i2c` group.
It does not automatically reboot the Pi. If the boot configuration changed,
reboot once to activate the bus:

```sh
ssh jojo@drummachine.lab.be
sudo reboot
```

After reconnecting, check the hardware with the service temporarily stopped:

```sh
sudo systemctl stop drum-machine
ls -l /dev/i2c-1
i2cdetect -y 1 0x20 0x21
sudo systemctl start drum-machine
```

The scan should show both `20` and `21`. If an address is absent, check power,
ground, SDA/SCL, RESET and the address straps before proceeding. If the device
file is absent after reboot, check that the configured firmware file actually
belongs to the active boot setup and that I²C is enabled there.

Deployment settings are in `deploy/ansible/group_vars/drum_machines.yml`:

```yaml
drum_machine_step_buttons_enabled: true
drum_machine_i2c_device: /dev/i2c-1
drum_machine_step_addresses: ['0x20', '0x21']
drum_machine_boot_config: /boot/firmware/config.txt
```

Set `drum_machine_step_buttons_enabled: false` to deploy a terminal-only service.
The Ansible playbook requires the firmware file to exist rather than silently
creating a file at an incorrect boot path.

## Select a track and edit steps

```sh
ssh jojo@drummachine.lab.be
drum-machine
```

```text
select 1
status
play
tempo 128
select 2
status
quit
```

The default selection is **track 1 (kick)**. Track 2 is snare, 3 hi-hat and
4 clap. The selected track appears in `status`. Each stable press toggles one
step in that track. Holding a button does not repeat. Presses and releases are
debounced for 20 ms, with nominal polling every 5 ms. A button already held
when a board connects must be released and pressed again before it edits a step.

Buttons work while stopped and during playback. `quit` or closing SSH only
disconnects the terminal; buttons and the engine continue working. Terminal
commands like `step 3 8` still explicitly edit their named track, without
changing the physical buttons' selected track.

The driver configures both modules exclusively. Do not run another GPIO driver
or use the spare pins as outputs while this service owns the boards. Configuration
is checked periodically; a reset or external configuration change triggers a
reinitialisation. Start with freshly powered modules in their default BANK=0
register layout; a previous program that changed BANK must be reset/power-cycled.

Missing boards do not terminate audio or terminal operation. Each board is
retried independently once per second, and held levels are primed on recovery
to avoid phantom presses. Inspect errors with:

```sh
sudo journalctl -u drum-machine -f
```

For manual diagnostics with the actual executable (the installed launcher is a
client), use `--serve --step-buttons`, optionally `--i2c-device PATH` and
`--step-addresses 0x20 0x21`. No LEDs, sliders or physical track-select buttons
are driven by this change. Pattern and track selection still reset on restart.

## References

- [Purchased module listing](https://www.amazon.com.be/dp/B0DGKKR9BV)
- [Microchip MCP23017 datasheet, including GPA7/GPB7 restrictions](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-MCP23S17-16-Bit-IO-Expander-with-Serial-Interface-DS20001952.pdf)
- [Raspberry Pi GPIO and I²C documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
- [Linux I²C userspace interface](https://docs.kernel.org/i2c/dev-interface.html)
