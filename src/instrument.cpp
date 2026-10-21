#include "instrument.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace drum {
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint16_t>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);
namespace {
bool validTrack(int track) { return track >= 0 && track < trackCount; }
}

Instrument::Instrument(Samples samples, int rate)
    : samples_(std::move(samples)), rate_(rate > 0 ? rate : sampleRate) {
    for (int track = 0; track < trackCount; ++track) {
        volumes_[track].store(0.75f);
        gains_[track] = 0.75f;
        positions_[track] = samples_[track].pcm.size();
    }
    loadDemo();
}

void Instrument::play() noexcept {
    playing_.store(true, std::memory_order_relaxed);
}

void Instrument::stop() noexcept {
    playing_.store(false, std::memory_order_relaxed);
}

void Instrument::togglePlayback() noexcept {
    playing_.store(!playing_.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void Instrument::setTempo(double bpm) noexcept {
    tempo_.store(std::isfinite(bpm) ? std::clamp(bpm, 40.0, 240.0) : 120.0, std::memory_order_relaxed);
}

void Instrument::setVolume(int track, float volume) noexcept {
    if (!validTrack(track)) return;
    const float gain = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    volumes_[track].store(gain, std::memory_order_relaxed);
}

void Instrument::toggleMute(int track) noexcept {
    if (!validTrack(track)) return;
    const float volume = volumes_[track].load(std::memory_order_relaxed);
    if (volume > 0) {
        rememberedVolumes_[track] = volume;
        setVolume(track, 0);
    } else {
        setVolume(track, rememberedVolumes_[track]);
    }
}

void Instrument::toggleStep(int track, int step) noexcept {
    if (validTrack(track) && step >= 0 && step < stepCount)
        patterns_[track].fetch_xor(static_cast<std::uint16_t>(1u << step), std::memory_order_relaxed);
}

void Instrument::selectTrack(int track) noexcept {
    if (validTrack(track)) selectedTrack_ = track;
}

void Instrument::toggleSelectedStep(int step) noexcept { toggleStep(selectedTrack_, step); }

void Instrument::clearPattern() noexcept {
    for (auto& pattern : patterns_) pattern.store(0, std::memory_order_relaxed);
}

void Instrument::loadDemo() noexcept {
    constexpr std::array<std::uint16_t, trackCount> demo{0x1111, 0x1010, 0x5555, 0x1000};
    for (int track = 0; track < trackCount; ++track) patterns_[track].store(demo[track], std::memory_order_relaxed);
}

void Instrument::preview(int track) noexcept {
    if (validTrack(track)) previewRequests_.fetch_or(1u << track, std::memory_order_relaxed);
}

Status Instrument::status() const noexcept {
    Status state;
    state.selectedTrack = selectedTrack_;
    state.playing = playing_.load(std::memory_order_relaxed);
    state.tempo = tempo_.load(std::memory_order_relaxed);
    state.currentStep = currentStep_.load(std::memory_order_relaxed);
    state.peak = peak_.load(std::memory_order_relaxed);
    for (int track = 0; track < trackCount; ++track) {
        const auto pattern = patterns_[track].load(std::memory_order_relaxed);
        for (int step = 0; step < stepCount; ++step) {
            state.tracks[track].steps[step] = (pattern & (1u << step)) != 0;
        }
        state.tracks[track].volume = volumes_[track].load(std::memory_order_relaxed);
        state.tracks[track].muted = state.tracks[track].volume == 0;
    }
    return state;
}

void Instrument::render(float* output, std::size_t frames) noexcept {
    const bool playing = playing_.load(std::memory_order_relaxed);
    const double requestedBpm = tempo_.load(std::memory_order_relaxed);
    const double bpm = std::isfinite(requestedBpm) ? std::clamp(requestedBpm, 40.0, 240.0) : 120.0;
    const double stepFrames = static_cast<double>(rate_) * 60.0 / (bpm * 4.0);
    std::array<std::uint16_t, trackCount> patterns;
    std::array<float, trackCount> targets;
    for (int track = 0; track < trackCount; ++track) {
        patterns[track] = patterns_[track].load(std::memory_order_relaxed);
        const float volume = volumes_[track].load(std::memory_order_relaxed);
        targets[track] = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    }

    if (playing != wasPlaying_) {
        framesUntilStep_ = 0.0;
        nextStep_ = 0;
        if (!playing) currentStep_.store(-1, std::memory_order_relaxed);
        wasPlaying_ = playing;
    } else if (previousStepFrames_ > 0.0) {
        framesUntilStep_ *= stepFrames / previousStepFrames_;
    }
    previousStepFrames_ = stepFrames;

    const unsigned previews = previewRequests_.exchange(0, std::memory_order_relaxed);
    for (int track = 0; track < trackCount; ++track)
        if (previews & (1u << track)) positions_[track] = 0;

    float peak = 0.0f;
    const float smoothing = 1.0f - std::exp(-1.0f / (0.003f * static_cast<float>(rate_)));
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (playing && framesUntilStep_ <= 0.0) {
            for (int track = 0; track < trackCount; ++track)
                if (patterns[track] & (1u << nextStep_)) positions_[track] = 0;
            currentStep_.store(nextStep_, std::memory_order_relaxed);
            nextStep_ = (nextStep_ + 1) % stepCount;
            framesUntilStep_ += stepFrames;
        }
        if (playing) framesUntilStep_ -= 1.0;
        float left = 0.0f, right = 0.0f;
        for (int track = 0; track < trackCount; ++track) {
            gains_[track] += (targets[track] - gains_[track]) * smoothing;
            const auto& pcm = samples_[track].pcm;
            auto& position = positions_[track];
            if (position + 1 < pcm.size()) {
                // Stop halts the sequencer; existing one-shots decay naturally.
                const float gain = gains_[track];
                left += pcm[position++] * gain;
                right += pcm[position++] * gain;
            }
        }
        // Soft saturation keeps simultaneous hits safely inside full scale.
        left = std::tanh(left * 0.7f);
        right = std::tanh(right * 0.7f);
        output[frame * 2] = left;
        output[frame * 2 + 1] = right;
        peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
    }
    peak_.store(peak, std::memory_order_relaxed);
}
} // namespace drum
