#include "options.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace drum {
Options parseOptions(int argc, char** argv) {
    Options options;
#ifndef DRUM_MACHINE_DESKTOP
    options.terminal = true;
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--terminal") options.terminal = true;
        else if (arg == "--smoke-test") options.smokeTest = true;
        else if (arg == "--help") options.help = true;
        else if ((arg == "--assets" || arg == "--render-demo" || arg == "--screenshot") && i + 1 < argc) {
            const std::filesystem::path path = argv[++i];
            if (arg == "--assets") options.assets = path;
            else if (arg == "--render-demo") options.renderDemo = path;
            else options.screenshot = path;
        } else throw std::runtime_error("Unknown or incomplete option: " + arg);
    }
    if (!options.help && options.terminal && (options.smokeTest || !options.screenshot.empty()))
        throw std::runtime_error("--smoke-test and --screenshot require the desktop frontend");
    return options;
}
void printHelp() {
    std::cout << "drum-machine\n"
        "  --terminal          Use line-based controls without a window\n"
        "  --assets DIR        Override bundled sample directory\n"
        "  --render-demo FILE  Render two bars to a stereo WAV and exit\n";
#ifdef DRUM_MACHINE_DESKTOP
    std::cout << "  --smoke-test        Check desktop controls with dummy audio/video\n"
                 "  --screenshot FILE   Save second frame as BMP\n"
                 "  Desktop: Space play/stop, 1-4 audition drums\n";
#endif
}
} // namespace drum
