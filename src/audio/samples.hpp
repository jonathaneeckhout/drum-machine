#pragma once

#include "instrument.hpp"
#include <filesystem>

namespace drum {
// Loading/conversion is startup work, outside the real-time instrument.
std::filesystem::path findAssets(const std::filesystem::path& overridePath = {});
Samples loadSamples(const std::filesystem::path& assets);
} // namespace drum
