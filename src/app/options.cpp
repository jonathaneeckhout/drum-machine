#include "options.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace drum {
Options parseOptions(int argc, char** argv) {
    Options options;
    int frontends = 0;
#ifndef DRUM_MACHINE_DESKTOP
    options.terminal = true;
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--terminal") { options.terminal = true; ++frontends; }
        else if (arg == "--serve") { options.serve = true; ++frontends; }
        else if (arg == "--connect") { options.connect = true; ++frontends; }
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
        "  --assets DIR        Override bundled sample directory\n"
        "  --render-demo FILE  Render two bars to a stereo WAV and exit\n";
#ifdef DRUM_MACHINE_DESKTOP
    std::cout << "  --smoke-test        Check desktop controls with dummy audio/video\n"
                 "  --screenshot FILE   Save second frame as BMP\n"
                 "  Desktop: Space play/stop, 1-4 audition drums\n";
#endif
}
} // namespace drum
