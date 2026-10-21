#include "terminal.hpp"
#include "instrument.hpp"

#include <cmath>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace drum {
namespace {
constexpr const char* help =
    "play | stop | tempo BPM | step TRACK STEP | volume TRACK LEVEL\n"
    "select TRACK | mute TRACK | preview TRACK | clear | demo | status | help | quit\n"
    "Tracks: 1=kick, 2=snare, 3=hi-hat, 4=clap. Steps: 1-16. Levels: 0-1.\n";
void show(const Instrument& instrument, std::ostream& out) {
    const auto state = instrument.status();
    out << (state.playing ? "playing" : "stopped") << " / " << state.tempo << " BPM\n";
    out << "selected track: " << state.selectedTrack + 1 << '\n';
    for (int track = 0; track < trackCount; ++track) {
        out << track + 1 << " ";
        for (bool step : state.tracks[track].steps) out << (step ? 'x' : '.');
        out << " / " << state.tracks[track].volume;
        if (state.tracks[track].muted) out << " muted";
        out << '\n';
    }
}
}

bool executeTerminalCommand(Instrument& instrument, std::string_view line, std::ostream& output) {
    std::istringstream command{std::string(line)};
    std::string verb;
    if (!(command >> verb)) return true;
    int track = 0, step = 0;
    double value = 0;
    // Parse completely before changing the instrument; reject trailing garbage.
    const auto end = [&] { command >> std::ws; return command.eof(); };
    const auto validTrack = [&] { return track >= 1 && track <= trackCount; };
    bool valid = true;
    if (verb == "play" && end()) instrument.play();
    else if (verb == "stop" && end()) instrument.stop();
    else if (verb == "clear" && end()) instrument.clearPattern();
    else if (verb == "demo" && end()) instrument.loadDemo();
    else if (verb == "status" && end()) { show(instrument, output); return true; }
    else if (verb == "help" && end()) { output << help; return true; }
    else if (verb == "quit" && end()) return false;
    else if (verb == "tempo" && (command >> value) && end() && std::isfinite(value) && value >= 40 && value <= 240)
        instrument.setTempo(value);
    else if (verb == "step" && (command >> track >> step) && end() && validTrack() && step >= 1 && step <= stepCount)
        instrument.toggleStep(track - 1, step - 1);
    else if (verb == "volume" && (command >> track >> value) && end() && validTrack() && std::isfinite(value) && value >= 0 && value <= 1)
        instrument.setVolume(track - 1, static_cast<float>(value));
    else if (verb == "mute" && (command >> track) && end() && validTrack()) instrument.toggleMute(track - 1);
    else if (verb == "select" && (command >> track) && end() && validTrack()) instrument.selectTrack(track - 1);
    else if (verb == "preview" && (command >> track) && end() && validTrack()) instrument.preview(track - 1);
    else valid = false;
    output << (valid ? "ok\n" : "Invalid command or value. Type help.\n");
    return true;
}

int runTerminal(Instrument& instrument, std::istream& input, std::ostream& output) {
    output << "drum-machine / terminal\n" << help;
    std::string line;
    while (output << "> " << std::flush, std::getline(input, line)) {
        if (!executeTerminalCommand(instrument, line, output)) break;
    }
    instrument.stop();
    return 0;
}
} // namespace drum
