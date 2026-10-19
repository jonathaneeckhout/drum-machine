#include "sdl_audio.hpp"
#include "instrument.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <stdexcept>

namespace drum {
namespace {
void require(bool ok, const char* context) {
    if (!ok) throw std::runtime_error(std::string(context) + ": " + SDL_GetError());
}
}

struct AudioOutput::Impl {
    Instrument& instrument;
    SDL_AudioStream* stream = nullptr;
    bool initialized = false;
    std::atomic<bool> failed{false};
    std::array<float, 1024> scratch{};
    explicit Impl(Instrument& source) : instrument(source) {}
    ~Impl() {
        if (stream) SDL_DestroyAudioStream(stream); // Joins callback before instrument destruction.
        if (initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional, int) {
        auto& audio = *static_cast<Impl*>(userdata);
        constexpr int bytesPerFrame = 2 * sizeof(float);
        int frames = (std::max(additional, 0) + bytesPerFrame - 1) / bytesPerFrame;
        while (frames > 0) {
            const int count = std::min(frames, 512);
            audio.instrument.render(audio.scratch.data(), static_cast<std::size_t>(count));
            if (!SDL_PutAudioStreamData(stream, audio.scratch.data(), count * bytesPerFrame)) {
                audio.failed.store(true);
                return;
            }
            frames -= count;
        }
    }
};

AudioOutput::AudioOutput(Instrument& instrument, bool dummyDevice)
    : impl_(std::make_unique<Impl>(instrument)) {
    if (dummyDevice) SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
    require(SDL_InitSubSystem(SDL_INIT_AUDIO), "Initialize audio");
    impl_->initialized = true;
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, sampleRate};
    impl_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Impl::callback, impl_.get());
    require(impl_->stream != nullptr, "Open audio output");
    require(SDL_ResumeAudioStreamDevice(impl_->stream), "Start audio output");
}
AudioOutput::~AudioOutput() = default;
bool AudioOutput::failed() const noexcept { return impl_->failed.load(); }
std::string AudioOutput::name() const {
    const auto device = SDL_GetAudioStreamDevice(impl_->stream);
    const char* name = SDL_GetAudioDeviceName(device);
    return name ? name : "Default audio device";
}
} // namespace drum
