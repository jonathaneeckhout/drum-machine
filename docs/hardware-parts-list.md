# Hardware parts list

This list covers the **currently implemented 16-step prototype**. Quantities
are totals required, not instructions to buy parts already owned. Use the
[combined wiring schema](hardware-wiring.md) when assembling it.

## Parts needed now

| Quantity | Item | Notes |
| ---: | --- | --- |
| 1 | Raspberry Pi 3 | Already owned; current deployment targets Debian 13 ARM64 |
| 1 | Suitable microSD card | With the supported OS and working SSH |
| 1 | Pi 3-compatible power supply | Reuse the working supply; check capacity for the complete assembly |
| 2 | MCP23017 / CJMCU-2317 modules | From the purchased pack; eight buttons and eight LEDs per module; 3.3 V operation, addresses `0x20`/`0x21` |
| 16 | Momentary normally open buttons | Reuse existing buttons |
| 16 | Single-colour, two-legged LEDs | This wiring assumes ordinary indicator LEDs; verify the actual type; red is a useful first test |
| 16 | 1 kΩ resistors | One series resistor per LED; required for this direct-drive circuit |
| As needed | Breadboard and jumper/hookup wires | Common 3.3 V/GND rails and short SDA/SCL wiring |
| As needed | Headers and removable connectors | Match the module headers and panel wiring |
| 1 set | Audio cable and speakers/headphones | Default deployment uses the Pi's headphone jack |

No ADC, OLED, encoder, extra GPIO expander or TLC5947 is required for the
current buttons and LEDs. Button pull-ups are internal to the MCP23017; no
separate button pull-up resistors are needed.

For an initial breadboard test, one module, one button, one LED and one
resistor are enough. With one module populated you can expand to eight steps.

## Enclosure and assembly materials

- MDF panels for the planned approximately 40 × 25 × 5 cm hinged enclosure;
  measure actual components and internal clearance before cutting.
- Suitable hinges, lid restraint/closure, panel screws and wood glue.
- Standoffs, screws and nuts matched to the Pi and module mounting holes.
- Perfboard or another secure soldered assembly for the final wiring.
- Insulated wire, heat-shrink, strain relief and cable ties.
- Optional LED holders matched to LED size and panel thickness.

Useful tools: multimeter, soldering iron, solder, cutters and wire stripper.
The exact panel thickness and mounting dimensions are not specified here.

## Assembly order

1. Verify Pi audio and SSH control with the existing service.
2. Connect one button and one LED on board `0x20`; follow the wiring guide's test.
3. Expand to eight steps, then add board `0x21` for all 16.
4. Check all controls and LEDs before soldering the permanent wiring.
5. Dry-fit boards and wiring in a panel mockup before finishing the enclosure.

## Future parts

The [eight-track plan](hardware-wiring-8-tracks.md) includes eight sliders and
additional select, mute and transport controls. Their ADC, expansion and LED
capacity still need allocating. The former four-track BOM and TLC5947 wiring
have been superseded; they are not a shopping list for the current prototype.
