#include "instrument.hpp"
#include "app/export.hpp"
#include "app/options.hpp"
#include "audio/samples.hpp"
#include "audio/sdl_audio.hpp"
#include "frontends/terminal.hpp"
#include "frontends/service.hpp"
#ifdef DRUM_MACHINE_DESKTOP
#include "frontends/desktop.hpp"
#endif

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        const auto options = drum::parseOptions(argc, argv);
        if (options.help) { drum::printHelp(); return 0; }
        const auto socket = options.socket.empty() ? drum::defaultControlSocket() : options.socket;
        if (options.connect) return drum::runClient(socket, std::cin, std::cout, options.command);
        drum::Instrument instrument(drum::loadSamples(drum::findAssets(options.assets)));
        if (!options.renderDemo.empty()) {
            drum::writeDemo(options.renderDemo, instrument);
            std::cout << "Rendered two bars to " << options.renderDemo << '\n';
            return 0;
        }
        drum::AudioOutput audio(instrument, options.smokeTest);
        int result = 0;
        if (options.serve) result = drum::runService(instrument, socket, [&audio] { return audio.failed(); });
        else if (options.terminal) result = drum::runTerminal(instrument, std::cin, std::cout);
#ifdef DRUM_MACHINE_DESKTOP
        else result = drum::runDesktop(instrument, audio, {options.smokeTest, options.screenshot});
#endif
        if (audio.failed()) throw std::runtime_error("Audio stream failed");
        return result;
    } catch (const std::exception& error) {
        std::cerr << "drum-machine: " << error.what() << '\n';
        return 1;
    }
}
