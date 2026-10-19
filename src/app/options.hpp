#pragma once
#include <filesystem>

namespace drum {
struct Options {
    bool terminal = false;
    bool smokeTest = false;
    bool help = false;
    std::filesystem::path assets, renderDemo, screenshot;
};
Options parseOptions(int argc, char** argv);
void printHelp();
} // namespace drum
