# Eight-track hardware connection proposal

18 October 2026. Covers the user's revised list: 16 step buttons, 16 step LEDs,
8 select buttons, 8 selection LEDs, 8 volume sliders, 8 mute buttons, Play,
Stop, Next sequence and Previous sequence. Totals: 36 buttons, 24 LEDs and
8 analog inputs. Mute LEDs are not included in this revised list.

The earlier display, encoder, Clear and Shutdown controls are outside this
connection proposal; omission does not implement or remove them in software.

## Interface parts

| Quantity | Item | Capacity used |
| ---: | --- | --- |
| 3 | MCP23017 I²C breakout | 12 button inputs per board; 36 total |
| 1 | MCP3008 ADC, 3.3 V operation | All 8 channels for sliders |
| 1 | Adafruit TLC5947 breakout, product 1429 | All 24 LED channels |
| 8 | 10 kΩ linear single-gang sliders | Recommended control potentiometers |
| 36 | Momentary normally open buttons | Reuse existing compatible buttons |
| 24 | Single-colour, two-lead LEDs | 16 steps plus 8 selection indicators |

Also required: Pi 3, SD card, suitable power supply, wiring, connectors,
decoupling, prototype board and an audio output arrangement. Final component
dimensions, LED ratings and power budget remain to be checked.

## Pi header allocation

BCM numbers and physical header positions are different numbering systems.
This proposal assumes the Pi 3's standard 40-pin header and the named breakouts.

| Pi BCM GPIO | Physical pin | Destination |
| ---: | ---: | --- |
| 2 / SDA1 | 3 | SDA on all three MCP23017 boards |
| 3 / SCL1 | 5 | SCL on all three MCP23017 boards |
| 11 / SPI0 SCLK | 23 | MCP3008 CLK |
| 10 / SPI0 MOSI | 19 | MCP3008 DIN |
| 9 / SPI0 MISO | 21 | MCP3008 DOUT |
| 8 / SPI0 CE0 | 24 | MCP3008 CS/SHDN |
| 17 | 11 | TLC5947 DIN |
| 27 | 13 | TLC5947 CLK |
| 22 | 15 | TLC5947 LAT |

Power MCP23017 boards at 3.3 V. Set their RESET inputs inactive/high according
to the breakout's schematic (normally handled by the board's pull-up).
Power MCP3008 VDD and VREF from 3.3 V; connect AGND and DGND to ground.
Use local supply decoupling, especially with a bare ADC.

Power the Adafruit TLC5947 board's V+ from 5 V. Its control inputs accept the
Pi's 3.3 V logic. Connect all grounds together. Pi header physical pin 1 or 17
provides 3.3 V; pin 2 or 4 provides 5 V; pin 6 is one ground option.
Power rails are not GPIO outputs. Check supply capacity before powering the
complete assembly from the Pi header.

Use separate GPIO clock/data for TLC5947 because it has no conventional
chip-select to isolate ADC SPI traffic. Its OE pin is active high for blanking;
the reference breakout pulls it low. An optional OE control/pull-up circuit can
keep LEDs dark until software has initialised them.

## Buttons

All MCP23017 boards share SDA and SCL. Set address straps explicitly:

| Address | A2 A1 A0 | Buttons |
| --- | --- | --- |
| 0x20 | 0 0 0 | Steps 1–12 |
| 0x21 | 0 0 1 | Steps 13–16, select tracks 1–8 |
| 0x22 | 0 1 0 | Mute tracks 1–8, Play, Stop, Next, Previous |

On each board use GPA0–GPA6 then GPB0–GPB4, in that order, for the 12
buttons listed above. GPB5 and GPB6 remain spare on each board: six spare
button inputs total. Configure unused inputs with defined levels.

Reserve GPA7 and GPB7 as outputs, with no buttons connected, to accommodate
Microchip's documented input restriction. Never assume all 16 pins are usable
as inputs.

Connect each input to one button terminal; connect the other terminal to GND.
Enable the expander's internal pull-up: released reads high, pressed reads low.
Debounce in software. No button matrix or per-button diodes are needed.
Keep I²C pull-ups at 3.3 V and check their combined strength across breakouts.

## Sliders

For each slider, connect the two ends of the resistive track to 3.3 V and GND,
and the wiper to its ADC channel. Identify terminals from the slider's pinout
or a meter; physical pin order varies. CH0–CH7 map to track volumes 1–8.
Swap the outer connections if physical slider direction is reversed.
Do not feed 5 V into these analog inputs. Audio does not pass through the sliders.
Filter slider readings in software; evaluate analog filtering with actual wiring.

## LEDs

Connect LED anodes to the driver's 5 V LED supply, and individual cathodes to
TLC5947 outputs: channels 0–15 for step LEDs 1–16; channels 16–23 for track
selection LEDs 1–8. The constant-current driver normally eliminates individual
series resistors. Set its current for the actual LEDs; start near 5 mA/channel
if supported by their ratings. The reference board defaults to about 15 mA,
so change its current-setting resistor as needed using the board's guidance.

At 5 mA each, 24 lit LEDs draw about 120 mA, excluding board overhead. These
channels are fully allocated. Adding 8 dedicated mute LEDs would require more
output capacity; do not order a second driver unless that feature is desired.

## Software implications

Hardware scanning, debounce, ADC reads and LED updates belong in a control loop
independent of the audio callback. Start with frequent button polling and measure
responsiveness, bus loading and audio dropouts on the Pi before tuning rates.

The existing four-track engine must be extended to eight tracks. Sequence
storage and next/previous selection also require software work. Decide separately
whether changes happen immediately or at a bar boundary; the wiring supports
either policy. This document does not claim software implementation or electrical
bench verification.

## References

- [MCP23017 breakout](https://www.adafruit.com/product/5346)
- [MCP23017 GPA7/GPB7 input restriction](https://support.microchip.com/s/article/GPA7---GPB7-Cannot-Be-Used-as-Inputs-In-MCP23017)
- [MCP3008 Pi wiring](https://learn.adafruit.com/mcp3008-spi-adc/python-circuitpython)
- [TLC5947 breakout, logic levels and current setting](https://www.adafruit.com/product/1429)
- [Pi GPIO documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html)
