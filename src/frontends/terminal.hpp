#pragma once
#include <iosfwd>
#include <string_view>

namespace drum {
class Instrument;
// Does not own playback lifetime. False means the caller should disconnect.
bool executeTerminalCommand(Instrument& instrument, std::string_view line, std::ostream& output);
// The stream arguments also allow scripted hardware-independent control tests.
int runTerminal(Instrument& instrument, std::istream& input, std::ostream& output);
} // namespace drum
