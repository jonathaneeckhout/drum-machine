#include "samples.hpp"
#include <SDL3/SDL.h>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;
namespace drum {
namespace {
constexpr std::array<const char*, 4> files{"kick.wav", "snare.wav", "hat.wav", "clap.wav"};
void require(bool ok, const char* context) {
    if (!ok) throw std::runtime_error(std::string(context) + ": " + SDL_GetError());
}
}
fs::path findAssets(const fs::path& overridePath) {
    if (!overridePath.empty()) return overridePath;
    const char* basePath = SDL_GetBasePath();
    require(basePath != nullptr, "Find executable directory");
    const fs::path base = basePath;
    const auto local = base / "assets";
    if (fs::exists(local)) return local;
    return base / ".." / "share" / "drum-machine" / "assets";
}
std::array<drum::Sample, 4> loadSamples(const fs::path& assets) {
    std::array<drum::Sample, 4> samples;
    const SDL_AudioSpec target{SDL_AUDIO_F32, 2, drum::sampleRate};
    for (int i = 0; i < 4; ++i) {
        const auto path = assets / "samples" / files[i];
        SDL_AudioSpec source{};
        Uint8* bytes = nullptr;
        Uint32 length = 0;
        require(SDL_LoadWAV(path.c_str(), &source, &bytes, &length), path.c_str());
        std::unique_ptr<Uint8, decltype(&SDL_free)> sourceOwner(bytes, SDL_free);
        Uint8* converted = nullptr;
        int convertedLength = 0;
        require(SDL_ConvertAudioSamples(&source, bytes, static_cast<int>(length), &target,
                                       &converted, &convertedLength), "Convert sample");
        std::unique_ptr<Uint8, decltype(&SDL_free)> convertedOwner(converted, SDL_free);
        samples[i].pcm.resize(static_cast<std::size_t>(convertedLength) / sizeof(float));
        std::memcpy(samples[i].pcm.data(), converted, static_cast<std::size_t>(convertedLength));
    }
    return samples;
}

} // namespace drum
