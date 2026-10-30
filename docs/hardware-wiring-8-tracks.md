# Future eight-track panel plan

**Future work: this is not an implemented wiring guide.** For the working Pi,
16 step buttons and 16 LEDs, use the [combined wiring schema](hardware-wiring.md).
The software currently supports four tracks.

This records the revised control list. It replaces the old proposal that used
12 buttons per MCP23017 and a separate TLC5947. Those assignments conflict with
the implemented step wiring and have been removed.

## Target controls

| Control | Quantity | Current support |
| --- | ---: | --- |
| Step buttons | 16 | Implemented on two MCP23017 modules |
| Step LEDs | 16 | Implemented on the same two modules |
| Track-select buttons | 8 | Not implemented; use CLI `select 1`–`select 4` for now |
| Track-selection LEDs | 8 | Not implemented |
| Volume sliders | 8 | Not implemented; volume uses CLI/desktop controls |
| Mute buttons | 8 | Not implemented; mute uses CLI/desktop controls |
| Play / Stop buttons | 2 | Not implemented; use CLI/desktop controls |
| Next / Previous sequence buttons | 2 | Not implemented; there is one in-memory pattern |

Totals for the proposed panel: **36 buttons, 24 LEDs and 8 analog inputs**.
Relative to the working prototype, that adds 20 buttons, 8 LEDs and 8 analog
inputs. Dedicated mute LEDs are not included.

## Expansion work before assigning pins

The existing two MCP23017 modules are fully allocated to step buttons and LEDs.
Preserve their wiring unless a deliberate hardware and driver migration is
planned. Additional controls require extra capacity and software support.
Do not apply the removed three-board, 12-input allocation to the current driver.

- Extend the engine, CLI and sample set from four to eight tracks.
- Choose an ADC for eight sliders. An MCP3008 is a candidate; it is not
  supported by the current application.
- Budget GPIO/driver capacity for 20 extra buttons and 8 selection LEDs,
  including total LED current and the expansion boards' usable input pins.
- Decide sequence storage and whether navigation switches immediately or at
  the next bar boundary. Persistence does not exist yet.
- Add scanning, debounce, slider filtering and feedback on the control thread,
  outside the audio callback. Measure behaviour on the actual Pi.

If 10 kΩ linear sliders are chosen, they act as control potentiometers:
software translates ADC readings into volume. Audio does not pass through them.
Confirm resistance, travel, terminals and panel fit before assigning connections.
There is no final ADC or extra-control pin allocation in this document.

The earlier OLED, tempo encoder, Clear/Shutdown buttons and mute indicators
are optional ideas outside this revised list. Adding them changes the expansion
budget and needs its own design decision.
