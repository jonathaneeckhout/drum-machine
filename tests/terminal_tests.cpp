#include "frontends/terminal.hpp"
#include "instrument.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}
int main() {
    drum::Samples samples;
    for (auto& sample : samples) sample.pcm = {1, 1};
    drum::Instrument instrument(std::move(samples));
    std::istringstream input("clear\nstep 1 16\ntempo 150\nvolume 1 0.4\nmute 1\nmute 1\nplay\nstatus\n");
    std::ostringstream output;
    check(drum::runTerminal(instrument, input, output) == 0, "script completes");
    const auto state = instrument.status();
    check(state.tracks[0].steps[15] && !state.tracks[0].steps[0], "terminal uses one-based track/step numbers");
    check(state.tempo == 150 && std::abs(state.tracks[0].volume - 0.4f) < 0.0001f, "terminal commands reach instrument");
    check(!state.playing, "EOF stops playback");
    check(output.str().find("playing / 150 BPM") != std::string::npos, "terminal reports instrument status");
    std::istringstream invalid("step 0 1\nstep 1 17\nstep 1 16 garbage\ntempo 0\nvolume 1 2\nplay garbage\nquit\n");
    std::ostringstream errors;
    drum::runTerminal(instrument, invalid, errors);
    check(instrument.status().tracks[0].steps == state.tracks[0].steps && instrument.status().tempo == 150,
          "malformed commands cannot partially modify instrument state");
    check(errors.str().find("Invalid command") != std::string::npos, "terminal explains invalid input");
    std::istringstream preview("clear\npreview 2\nquit\n");
    drum::runTerminal(instrument, preview, output);
    float pcm[4]{};
    instrument.render(pcm, 2);
    check(pcm[0] > 0 && pcm[2] == 0, "terminal preview produces a single audible hit");
    std::cout << "Terminal command tests passed.\n";
}
