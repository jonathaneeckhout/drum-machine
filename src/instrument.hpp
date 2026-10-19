#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace drum {
constexpr int trackCount = 4;
constexpr int stepCount = 16;
constexpr int sampleRate = 48000;

// Stereo, interleaved float PCM. Supplied once, before audio starts.
struct Sample { std::vector<float> pcm; };
using Samples = std::array<Sample, trackCount>;

struct TrackStatus {
    std::array<bool, stepCount> steps{};
    float volume = 0.0f;
    bool muted = false;
};
struct Status {
    bool playing = false;
    double tempo = 120.0;
    std::array<TrackStatus, trackCount> tracks{};
    int currentStep = -1;
    float peak = 0.0f;
};

// One serialized control thread calls commands/status; one audio thread calls
// render. They may run concurrently. Status fields are independent observations,
// not a transaction. Indices are zero-based; invalid indices are ignored.
class Instrument {
public:
    explicit Instrument(Samples samples, int rate = sampleRate);
    void play() noexcept;
    void stop() noexcept;
    void togglePlayback() noexcept;
    void setTempo(double bpm) noexcept; // 40..240; non-finite values become 120.
    void setVolume(int track, float volume) noexcept; // 0..1; non-finite becomes 0.
    void toggleMute(int track) noexcept; // Restore the last nonzero level on unmute.
    void toggleStep(int track, int step) noexcept;
    void clearPattern() noexcept;
    void loadDemo() noexcept; // Pattern only; preserves tempo and levels.
    void preview(int track) noexcept;
    Status status() const noexcept;

    // Called by the audio adapter (or offline exporter), never by a frontend.
    // Writes `frames * 2` floats. No allocation, I/O, or blocking locks.
    void render(float* output, std::size_t frames) noexcept;

private:
    std::atomic<bool> playing_{false};
    std::atomic<double> tempo_{120.0};
    std::array<std::atomic<std::uint16_t>, trackCount> patterns_{};
    std::array<std::atomic<float>, trackCount> volumes_{};
    std::atomic<unsigned> previewRequests_{0};
    std::atomic<int> currentStep_{-1};
    std::atomic<float> peak_{0.0f};
    std::array<float, trackCount> rememberedVolumes_{0.75f, 0.75f, 0.75f, 0.75f};

    const Samples samples_;
    const int rate_;
    std::array<std::size_t, trackCount> positions_{};
    std::array<float, trackCount> gains_{};
    bool wasPlaying_ = false;
    double framesUntilStep_ = 0.0;
    double previousStepFrames_ = 0.0;
    int nextStep_ = 0;
};
} // namespace drum
