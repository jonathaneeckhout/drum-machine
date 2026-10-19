# Hardware parts list

> Historical four-track BOM. For the revised eight-track panel, use the quantities
> and connections in [hardware-wiring-8-tracks.md](hardware-wiring-8-tracks.md).

Prototype bill of materials for the agreed four-track, 16-step design.
Prepared 17 October 2026. Quantities are totals needed, not necessarily quantities
to buy: reuse existing parts first. Links identify reference products, not a
required retailer. Prices, Belgian/EU availability and delivery are not quoted.

## Controls and indicators

| Quantity | Item | Specification / use |
| ---: | --- | --- |
| 1 | Raspberry Pi 3 | Already owned; confirm exact variant before enclosure layout |
| 1 | microSD card | 16–32 GB recommended; reuse an existing suitable card |
| 28 | Momentary, normally open buttons | Reuse existing buttons: 16 steps, 4 track select, 4 mute, Play, Stop, Clear, Shutdown; encoder push switch is additional |
| 4 | Slide potentiometers | Recommended: 10 kΩ linear, single gang, non-motorised, approximately 45–60 mm travel; verify dimensions and pin arrangement before ordering |
| 4 | Slider caps | Must fit the selected slider levers |
| 1 | Rotary encoder with push switch | Detented quadrature encoder, panel mount; [Adafruit 377](https://www.adafruit.com/product/377) is a reference |
| 1 | Encoder knob | Match the shaft; included with reference encoder |
| 24 | Ordinary single-colour LEDs | 16 step, 4 track-select and 4 mute indicators; reuse available two-lead LEDs; 3 mm or 5 mm to suit panel |
| Up to 24 | LED panel holders | Optional; size to match LEDs and panel thickness |
| 1 | Small OLED display module | 1.3-inch 128×64, I²C, 3.3 V-compatible logic; [Adafruit 938](https://www.adafruit.com/product/938) is an SSD1306 reference module |

Linear faders are appropriate because these are control inputs: software can
apply a suitable volume curve. Audio does not pass through the faders.

## Interface electronics

| Quantity | Item | Specification / use |
| ---: | --- | --- |
| 1 | MCP3008 ADC | Eight analog inputs, four used for sliders; use a 3.3 V-compatible breakout or DIP chip with socket and local decoupling; [reference chip](https://www.adafruit.com/product/856) |
| 2 | MCP23017 GPIO expander breakout boards | Power and logic at 3.3 V; independently selectable I²C addresses; [reference board](https://www.adafruit.com/product/5346) |
| 1 | TLC5947 24-channel LED driver breakout | All 24 indicators; accepts Pi 3.3 V control signals; [reference board](https://www.adafruit.com/product/1429) |
| 1 pack | 100 nF ceramic capacitors | ADC supply decoupling if using a bare chip; slider filtering as determined during testing; breakouts may already include supply capacitors |
| 1 pack | 10 kΩ resistors | Spare pull-ups / input conditioning where required; expanders have configurable internal pull-ups |
| 1 pack | 1 kΩ resistors | For limited-current direct LED tests and prototype input filtering; not required in series with every TLC5947-driven LED |
| 1 | LED driver current-setting resistor | Select for the actual LEDs; target approximately 5 mA/channel initially; calculate against the chosen board/datasheet before fitting |

### Input allocation and compatibility

Use only GPA0–GPA6 and GPB0–GPB6 as button inputs on each MCP23017: 14 inputs
per board, 28 total. Reserve GPA7 and GPB7 as outputs, following the
[manufacturer's input restriction](https://support.microchip.com/s/article/GPA7---GPB7-Cannot-Be-Used-as-Inputs-In-MCP23017).
Do not wire buttons to those four excluded pins across the two boards.

Connect 27 ordinary buttons through the expanders and reserve a direct GPIO
for Shutdown. That leaves one usable expander input spare. Connect the encoder's
A/B signals and push switch directly to GPIO. The shutdown/wake wiring needs
finalising separately; shutdown alone does not physically disconnect power.

Set different expander addresses, such as 0x20 and 0x21. The reference display
uses 0x3C or 0x3D. Power I²C pull-ups from 3.3 V, and check their combined
resistance when several breakout boards share the bus.

The MCP3008 uses SPI. Give the TLC5947 its own data/clock/latch control pins
in the initial wiring plan: its serial input has no ordinary chip-select and
must not accidentally latch traffic intended for the ADC. Both interfaces fit
alongside the proposed direct GPIO controls, with final pins still to assign.

The TLC5947 reference board takes a 5 V supply and accepts 3–5 V control
signals. Its default current is approximately 15 mA per channel; lower the
current setting if appropriate for the reused LEDs and desired brightness.
It regulates LED current itself, so individual series resistors are normally
unnecessary on its outputs. Do not connect an LED directly to a Pi output
without current limiting. This list assumes single-colour LEDs, not addressable
or RGB LEDs.

## Audio, power and construction

| Quantity | Item | Specification / use |
| ---: | --- | --- |
| 1 | USB stereo audio interface / DAC | For final line output: Linux ALSA / USB Audio Class support, stereo playback at 48 kHz and actual line outputs; choose model after budgeting and latency checks |
| 1 set | Audio cables | Match interface and speakers/mixer; preferably use the interface's existing line connectors initially |
| 1 | Pi-compatible wall supply | Official-style 5.1 V, 2.5 A micro-USB supply is the Pi 3 baseline; check total consumption with LEDs, display and USB audio before final assembly |
| 1 | Short USB cable | Only if required by the selected audio interface |
| 1 | Solderless breadboard | Initial interface and control experiments |
| 1–2 | Perfboards / stripboards | Soldered prototype assembly; size after panel mockup |
| 1 set | Jumper and hookup wire | Breadboard jumpers plus stranded wire for panel controls |
| 1 set | Headers and removable connectors | Match selected modules; enough for panel controls and power connections |
| 1 set | Heat-shrink tubing and cable ties | Insulation and strain relief |
| 1 set | M2.5/M3 standoffs, screws and nuts | Match Pi, module and panel mounting holes |
| 1 | Cardboard panel mockup | Check hand spacing, slider travel and button grouping |
| 1 | Enclosure / front panel | Buy or fabricate after measuring actual controls; leave Pi ventilation and SD/service access |

Use the Pi's onboard audio for early experiments if its quality is sufficient;
the final USB interface can be deferred. A headphone-only dongle is not
automatically an equivalent substitute for a stereo line-output interface.

At 5 mA per channel, the 24 LEDs alone draw up to approximately 120 mA, before
driver overhead. Include that and the selected USB interface in the power budget.
Use common ground for the Pi and peripheral electronics, and keep LED current
return wiring away from analog slider signals where practical.

Useful tools if not already available: soldering iron, solder, wire cutters,
wire stripper and multimeter. They are not additional instrument components.

## Suggested assembly order

1. Run the existing audio software on the Pi and check for audio dropouts.
2. Test one reused button, one LED and one slider with the interface electronics.
3. Connect all 16 steps and the four track strips.
4. Add the encoder, display, save/shutdown behaviour and final audio interface.
5. Build the enclosure after the cardboard layout and wiring have been tested.

## Sources

- [MCP3008 specifications](https://www.microchip.com/en-us/product/MCP3008)
- [MCP23017 reference breakout](https://www.adafruit.com/product/5346)
- [MCP23017 GPA7/GPB7 restriction](https://support.microchip.com/s/article/GPA7---GPB7-Cannot-Be-Used-as-Inputs-In-MCP23017)
- [TLC5947 reference breakout and electrical details](https://www.adafruit.com/product/1429)
- [TLC5947 current-setting guidance](https://learn.adafruit.com/tlc5947-tlc59711-pwm-led-driver-breakout?view=all)
- [OLED reference module](https://www.adafruit.com/product/938)
- [Encoder reference](https://www.adafruit.com/product/377)
- [Raspberry Pi power and GPIO documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
