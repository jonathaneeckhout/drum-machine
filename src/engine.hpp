#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace drum {
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint16_t>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);
constexpr int trackCount = 4;
constexpr int stepCount = 16;
constexpr int sampleRate = 48000;

// Immutable stereo, interleaved float PCM, loaded before the audio thread starts.
struct Sample { std::vector<float> pcm; };

// UI/hardware writes controls; only the audio callback calls render().
struct Controls {
    std::atomic<bool> playing{false};
    std::atomic<double> bpm{120.0};
    std::array<std::atomic<std::uint16_t>, trackCount> patterns{};
    std::array<std::atomic<float>, trackCount> volumes{};
    std::atomic<unsigned> previewRequests{0};
    std::atomic<int> currentStep{-1};
    std::atomic<float> peak{0.0f};
    Controls();
};

class Engine {
public:
    Engine(Controls& controls, std::array<Sample, trackCount> samples,
           int rate = sampleRate);
    // Writes stereo frames. No allocation, locks, I/O, or UI dependencies.
    void render(float* output, std::size_t frames) noexcept;

private:
    Controls& controls_;
    const std::array<Sample, trackCount> samples_;
    const int rate_;
    std::array<std::size_t, trackCount> positions_{};
    std::array<float, trackCount> gains_{};
    bool wasPlaying_ = false;
    double framesUntilStep_ = 0.0;
    double previousStepFrames_ = 0.0;
    int nextStep_ = 0;
};
} // namespace drum
