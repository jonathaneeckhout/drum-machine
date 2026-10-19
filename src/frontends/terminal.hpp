#pragma once
#include <iosfwd>

namespace drum {
class Instrument;
// The stream arguments also allow scripted hardware-independent control tests.
int runTerminal(Instrument& instrument, std::istream& input, std::ostream& output);
} // namespace drum
