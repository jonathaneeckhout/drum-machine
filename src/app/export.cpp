#include "export.hpp"
#include "instrument.hpp"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace drum {
void writeDemo(const fs::path& path, Instrument& instrument) {
    instrument.play();
    constexpr int frames = drum::sampleRate * 4; // Two bars at 120 BPM.
    std::vector<float> pcm(frames * 2);
    instrument.render(pcm.data(), frames);
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write " + path.string());
    const auto u16 = [&out](unsigned value) { out.put(value & 255); out.put((value >> 8) & 255); };
    const auto u32 = [&u16](unsigned value) { u16(value & 65535); u16(value >> 16); };
    out.write("RIFF", 4); u32(36 + frames * 4); out.write("WAVEfmt ", 8);
    u32(16); u16(1); u16(2); u32(drum::sampleRate); u32(drum::sampleRate * 4);
    u16(4); u16(16); out.write("data", 4); u32(frames * 4);
    for (float value : pcm) u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lrint(value * 32767))));
    if (!out) throw std::runtime_error("Failed writing " + path.string());
}

} // namespace drum
