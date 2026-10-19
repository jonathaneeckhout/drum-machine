#pragma once
#include <filesystem>

namespace drum {
class Instrument;
class AudioOutput;
struct DesktopOptions {
    bool smokeTest = false;
    std::filesystem::path screenshot;
};
int runDesktop(Instrument& instrument, const AudioOutput& audio, const DesktopOptions& options);
} // namespace drum
