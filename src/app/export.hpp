#pragma once
#include <filesystem>

namespace drum {
class Instrument;
void writeDemo(const std::filesystem::path& path, Instrument& instrument);
} // namespace drum
