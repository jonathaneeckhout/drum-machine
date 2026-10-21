#include "options.hpp"
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string>

namespace drum {
namespace {
std::uint8_t parseAddress(std::string_view value) {
    int base = 10;
    if (value.starts_with("0x") || value.starts_with("0X")) { base = 16; value.remove_prefix(2); }
    unsigned address = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), address, base);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || address < 0x20 || address > 0x27)
        throw std::runtime_error("MCP23017 address must be in 0x20..0x27");
    return static_cast<std::uint8_t>(address);
}
}
Options parseOptions(int argc, char** argv) {
    Options options;
    int frontends = 0;
    bool hardwareOptions = false;
#ifndef DRUM_MACHINE_DESKTOP
    options.terminal = true;
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--terminal") { options.terminal = true; ++frontends; }
        else if (arg == "--serve") { options.serve = true; ++frontends; }
        else if (arg == "--connect") { options.connect = true; ++frontends; }
        else if (arg == "--step-buttons") options.stepButtons.enabled = true;
        else if (arg == "--i2c-device" && i + 1 < argc) {
            options.stepButtons.device = argv[++i];
            hardwareOptions = true;
            if (options.stepButtons.device.empty()) throw std::runtime_error("I2C device path cannot be empty");
        }
        else if (arg == "--step-addresses" && i + 2 < argc) {
            options.stepButtons.addresses[0] = parseAddress(argv[++i]);
            options.stepButtons.addresses[1] = parseAddress(argv[++i]);
            hardwareOptions = true;
            if (options.stepButtons.addresses[0] == options.stepButtons.addresses[1])
                throw std::runtime_error("Step boards need different I2C addresses");
        }
        else if (arg == "--command" && i + 1 < argc) {
            options.command = argv[++i];
            if (options.command.empty()) throw std::runtime_error("--command cannot be empty");
        }
        else if (arg == "--smoke-test") options.smokeTest = true;
        else if (arg == "--help") options.help = true;
        else if ((arg == "--assets" || arg == "--render-demo" || arg == "--screenshot" || arg == "--socket") && i + 1 < argc) {
            const std::filesystem::path path = argv[++i];
            if (arg == "--assets") options.assets = path;
            else if (arg == "--render-demo") options.renderDemo = path;
            else if (arg == "--screenshot") options.screenshot = path;
            else options.socket = path;
        } else throw std::runtime_error("Unknown or incomplete option: " + arg);
    }
    if (frontends > 1) throw std::runtime_error("Choose only one of --terminal, --serve, --connect");
    if ((options.stepButtons.enabled && !options.serve) || (hardwareOptions && !options.stepButtons.enabled))
        throw std::runtime_error("Step button options require --serve --step-buttons");
    if (!options.command.empty()) {
        if (frontends != 0 && !options.connect) throw std::runtime_error("--command requires --connect");
        options.connect = true;
    }
    if ((options.serve || options.connect) &&
        (options.smokeTest || !options.screenshot.empty() || !options.renderDemo.empty()))
        throw std::runtime_error("Service/client modes cannot render a demo or run desktop checks");
    if (!options.socket.empty() && !options.serve && !options.connect)
        throw std::runtime_error("--socket requires --serve or --connect");
    if (!options.help && options.terminal && (options.smokeTest || !options.screenshot.empty()))
        throw std::runtime_error("--smoke-test and --screenshot require the desktop frontend");
    return options;
}
void printHelp() {
    std::cout << "drum-machine\n"
        "  --terminal          Use line-based controls without a window\n"
        "  --serve             Run audio and a local control socket until stopped\n"
        "  --connect           Control the running service from this terminal\n"
        "  --command TEXT      Send one command to the service and exit\n"
        "  --socket PATH       Override DRUM_MACHINE_SOCKET or /run/drum-machine/control.sock\n"
        "  --step-buttons      Read two MCP23017 step-button boards in service mode\n"
        "  --i2c-device PATH   I2C bus device (default /dev/i2c-1)\n"
        "  --step-addresses A B  Board addresses (default 0x20 0x21)\n"
        "  --assets DIR        Override bundled sample directory\n"
        "  --render-demo FILE  Render two bars to a stereo WAV and exit\n";
#ifdef DRUM_MACHINE_DESKTOP
    std::cout << "  --smoke-test        Check desktop controls with dummy audio/video\n"
                 "  --screenshot FILE   Save second frame as BMP\n"
                 "  Desktop: Space play/stop, 1-4 audition drums\n";
#endif
}
} // namespace drum
