#include "hardware/step_buttons.hpp"
#include "instrument.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
using namespace std::chrono_literals;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
struct FakeBus : drum::I2cRegisters {
    std::array<std::array<std::uint8_t, 0x16>, 2> registers{};
    std::array<bool, 2> missing{};
    std::array<int, 2> attempts{};
    std::vector<std::pair<unsigned, unsigned>> writes;
    FakeBus() { setPressed(0, 0); setPressed(1, 0); }
    void setPressed(unsigned board, std::uint8_t bits) {
        registers[board][0x12] = static_cast<std::uint8_t>(~bits | 0x80);
        registers[board][0x13] = static_cast<std::uint8_t>(0xfe | ((~bits >> 7) & 1));
    }
    unsigned access(std::uint8_t address) {
        check(address == 0x20 || address == 0x21, "correct I2C address");
        const auto board = address - 0x20;
        ++attempts[board];
        if (missing[board]) throw std::runtime_error("device absent");
        return board;
    }
    void write(std::uint8_t address, std::uint8_t reg, std::span<const std::uint8_t> bytes) override {
        const auto board = access(address);
        writes.emplace_back(board, reg);
        if (reg == 0 && bytes[1] == 1)
            check(registers[board][0x14] == 0x80 && registers[board][0x15] == 0xfe,
                  "LED latches are off before outputs are enabled");
        for (std::size_t i = 0; i < bytes.size(); ++i) registers[board][reg + i] = bytes[i];
    }
    void read(std::uint8_t address, std::uint8_t reg, std::span<std::uint8_t> bytes) override {
        const auto board = access(address);
        for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = registers[board][reg + i];
    }
};
}

int main() {
    const auto start = drum::Mcp23017Steps::Clock::now();
    FakeBus bus;
    std::ostringstream log;
    drum::Mcp23017Steps buttons(bus, {0x20, 0x21}, log);
    auto poll = [&](int ms) { return buttons.poll(start + std::chrono::milliseconds(ms)); };
    check(poll(0) == 0, "initial levels do not create presses");
    for (const auto& registers : bus.registers) {
        check(registers[0] == 0x7f && registers[1] == 0x7f, "GPA7/GPB7 are outputs, safe pins inputs");
        check(registers[0x0c] == 0x7f && registers[0x0d] == 0x7f, "safe input pull-ups enabled");
        check(registers[0x14] == 0 && registers[0x15] == 0, "unused output latches low");
    }
    bus.setPressed(0, 1);
    check(poll(1) == 0, "press waits for debounce");
    bus.setPressed(0, 0);
    poll(10);
    bus.setPressed(0, 1);
    poll(15);
    check(poll(34) == 0 && poll(35) == 1, "bouncing contact generates one stable press");
    check(poll(100) == 0, "held button does not repeat");
    bus.setPressed(0, 0);
    poll(101);
    check(poll(121) == 0, "release does not toggle a step");
    bus.setPressed(0, 1);
    poll(130);
    check(poll(150) == 1, "second press works after debounced release");
    bus.setPressed(0, 0);
    poll(200); poll(220);
    bus.setPressed(0, 0x81);
    bus.setPressed(1, 0x81);
    poll(230);
    check(poll(250) == 0x8181, "GPA0 and GPB0 map to steps 1/8 and 9/16 simultaneously");
    bus.setPressed(0, 0); bus.setPressed(1, 0);
    poll(260); poll(280);
    bus.setPressed(0, 2); poll(290);
    bus.setPressed(0, 6); poll(300);
    check(poll(310) == 2 && poll(320) == 4, "each button has its own debounce timer");

    // GPIO pins outside the configured eight-button mapping must not create events.
    bus.registers[1][0x12] &= 0x7f;
    bus.registers[1][0x13] &= 0x01;
    poll(330);
    check(poll(350) == 0, "unused and restricted pins ignored");

    bus.missing[0] = true;
    poll(400);
    const auto attempts = bus.attempts[0];
    bus.setPressed(1, 4); poll(410);
    check(poll(430) == 0x0400, "second board continues when first board fails");
    check(bus.attempts[0] == attempts, "failed board is not hammered between retries");
    bus.missing[0] = false;
    bus.setPressed(0, 1);
    check(poll(1400) == 0, "reconnection primes held inputs without phantom presses");
    bus.setPressed(0, 0); poll(1410); poll(1430);
    bus.setPressed(0, 1); poll(1440);
    check(poll(1460) == 1, "reconnected board accepts fresh presses");
    bus.registers[0][0] = 0xff;
    check(poll(2400) == 0, "detect a board reset");
    check(poll(3400) == 0 && bus.registers[0][0] == 0x7f, "reset board is reconfigured");

    FakeBus held;
    held.setPressed(0, 0xff); held.setPressed(1, 0xff);
    drum::Mcp23017Steps initialHeld(held, {0x20, 0x21}, log);
    check(initialHeld.poll(start) == 0 && initialHeld.poll(start + 25ms) == 0,
          "buttons held at startup do not edit the pattern");

    FakeBus ledBus;
    {
        drum::Mcp23017Steps panel(ledBus, {0x20, 0x21}, log, true);
        panel.poll(start);
        for (const auto& r : ledBus.registers)
            check(r[0] == 0x7f && r[1] == 1 && r[0x0c] == 0x7f && r[0x0d] == 1,
                  "eight button inputs keep pull-ups while eight LED pins become outputs");
        for (unsigned step = 0; step < 16; ++step) {
            panel.setLeds(1u << step);
            panel.poll(start + 1ms);
            for (unsigned board = 0; board < 2; ++board) {
                const auto mask = static_cast<unsigned>((1u << step) >> (8 * board)) & 0xff;
                check(ledBus.registers[board][0x14] == ((mask & 1) ? 0 : 0x80) &&
                      ledBus.registers[board][0x15] == ((~mask) & 0xfe),
                      "each LED maps to exactly one active-low output on its own board");
            }
        }
        const auto writes = ledBus.writes.size();
        panel.poll(start + 2ms);
        check(ledBus.writes.size() == writes, "unchanged LED state avoids redundant writes");
        panel.setLeds(0xffff);
        ledBus.setPressed(0, 0x81); ledBus.setPressed(1, 0x81);
        panel.poll(start + 3ms);
        check(panel.poll(start + 23ms) == 0x8181, "LED outputs do not interfere with button presses");
        ledBus.missing[1] = true;
        panel.poll(start + 24ms);
        panel.setLeds(0x0101);
        panel.poll(start + 25ms);
        check(ledBus.registers[0][0x14] == 0 && ledBus.registers[0][0x15] == 0xfe,
              "first board LEDs work with only one module connected");
        ledBus.missing[1] = false;
        panel.poll(start + 1024ms);
        check(ledBus.registers[1][0x14] == 0 && ledBus.registers[1][0x15] == 0xfe,
              "reconnected board receives current LED state");
        ledBus.registers[0][1] = 0xff;
        panel.poll(start + 2024ms);
        panel.poll(start + 3024ms);
        check(ledBus.registers[0][1] == 1 && ledBus.registers[0][0x14] == 0,
              "reset board restores output directions and current LED state");
    }
    for (const auto& r : ledBus.registers)
        check(r[0x14] == 0x80 && r[0x15] == 0xfe, "shutdown turns LEDs off");

    drum::Instrument instrument(drum::Samples{});
    instrument.clearPattern();
    check(instrument.status().selectedTrack == 0, "track 1 selected by default");
    instrument.toggleSelectedStep(15);
    instrument.selectTrack(2);
    instrument.toggleSelectedStep(7);
    instrument.selectTrack(-1);
    instrument.selectTrack(drum::trackCount);
    const auto state = instrument.status();
    check(state.selectedTrack == 2 && state.tracks[0].steps[15] && state.tracks[2].steps[7],
          "physical presses target selected track and invalid selections are ignored");
    std::cout << "Step button tests passed.\n";
}
