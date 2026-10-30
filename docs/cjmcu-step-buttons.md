# CJMCU-2317 step-button operation and diagnostics

For Pi connections, address straps and every button/LED pin assignment, use
**[the combined wiring schema](hardware-wiring.md)**. It covers the purchased
MCP23017 / CJMCU-2317 modules: eight buttons and eight LEDs per board, at
`0x20` and `0x21`. The engine currently has four tracks.

Buttons connect to ground and use internal pull-ups; no external button
pull-up resistors are needed. Do not connect buttons to GPA7 or GPB7.
In button-only mode those two pins are LOW outputs and should be unconnected;
GPB1–GPB6 are unused inputs. With LED support enabled, all eight pins not used
by buttons become LED outputs. Do not attach other circuits to those outputs.

## Deploy and enable I²C

From the development computer, build and deploy the updated application:

```sh
./tools/deploy-rpi3.sh --ask-become-pass
```

Ansible enables step buttons and LEDs by default for the configured Pi. It installs
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

With both modules connected, the scan should show `20` and `21`.
With only board `0x20`, just `20` is expected. If an address is absent, check power,
ground, SDA/SCL, RESET and the address straps before proceeding. If the device
file is absent after reboot, check that the configured firmware file actually
belongs to the active boot setup and that I²C is enabled there.

Deployment settings are in `deploy/ansible/group_vars/drum_machines.yml`:

```yaml
drum_machine_step_buttons_enabled: true
drum_machine_step_leds_enabled: true
drum_machine_i2c_device: /dev/i2c-1
drum_machine_step_addresses: ['0x20', '0x21']
drum_machine_boot_config: /boot/firmware/config.txt
```

Set both `drum_machine_step_buttons_enabled: false` and
`drum_machine_step_leds_enabled: false` to deploy a terminal-only service.
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
or configure pins independently while this service owns the boards.
Configuration is checked periodically; a reset or external configuration change triggers a
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
`--step-addresses 0x20 0x21`. Use `--serve --step-leds` to enable both buttons
and LEDs. See [LED operation](cjmcu-step-leds.md). Sliders and physical
track-select buttons are not implemented. Pattern and track selection reset
on service restart.

## References

- [Purchased module listing](https://www.amazon.com.be/dp/B0DGKKR9BV)
- [Microchip MCP23017 datasheet, including GPA7/GPB7 restrictions](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-MCP23S17-16-Bit-IO-Expander-with-Serial-Interface-DS20001952.pdf)
- [Raspberry Pi GPIO and I²C documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
- [Linux I²C userspace interface](https://docs.kernel.org/i2c/dev-interface.html)
