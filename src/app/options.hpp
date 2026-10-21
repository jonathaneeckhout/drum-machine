#pragma once
#include "hardware/step_buttons.hpp"
#include <filesystem>
#include <string>

namespace drum {
struct Options {
    bool terminal = false;
    bool serve = false;
    bool connect = false;
    bool smokeTest = false;
    bool help = false;
    std::filesystem::path assets, renderDemo, screenshot;
    std::filesystem::path socket;
    std::string command;
    StepButtonConfig stepButtons;
};
Options parseOptions(int argc, char** argv);
void printHelp();
} // namespace drum
