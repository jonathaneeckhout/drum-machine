#include "engine.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace drum {
Controls::Controls() {
    for (auto& volume : volumes) volume.store(0.75f);
    // A simple four-on-the-floor groove. Bit zero is step one.
    patterns[0].store(0x1111);
    patterns[1].store(0x1010);
    patterns[2].store(0x5555);
    patterns[3].store(0x1000);
}

Engine::Engine(Controls& controls, std::array<Sample, trackCount> samples, int rate)
    : controls_(controls), samples_(std::move(samples)), rate_(rate > 0 ? rate : sampleRate) {
    for (int track = 0; track < trackCount; ++track) {
        positions_[track] = samples_[track].pcm.size();
        const float volume = controls_.volumes[track].load();
        gains_[track] = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    }
}

void Engine::render(float* output, std::size_t frames) noexcept {
    const bool playing = controls_.playing.load(std::memory_order_relaxed);
    const double requestedBpm = controls_.bpm.load(std::memory_order_relaxed);
    const double bpm = std::isfinite(requestedBpm) ? std::clamp(requestedBpm, 40.0, 240.0) : 120.0;
    const double stepFrames = static_cast<double>(rate_) * 60.0 / (bpm * 4.0);
    std::array<std::uint16_t, trackCount> patterns;
    std::array<float, trackCount> targets;
    for (int track = 0; track < trackCount; ++track) {
        patterns[track] = controls_.patterns[track].load(std::memory_order_relaxed);
        const float volume = controls_.volumes[track].load(std::memory_order_relaxed);
        targets[track] = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    }

    if (playing != wasPlaying_) {
        framesUntilStep_ = 0.0;
        nextStep_ = 0;
        if (!playing) controls_.currentStep.store(-1, std::memory_order_relaxed);
        wasPlaying_ = playing;
    } else if (previousStepFrames_ > 0.0) {
        framesUntilStep_ *= stepFrames / previousStepFrames_;
    }
    previousStepFrames_ = stepFrames;

    const unsigned previews = controls_.previewRequests.exchange(0, std::memory_order_relaxed);
    for (int track = 0; track < trackCount; ++track)
        if (previews & (1u << track)) positions_[track] = 0;

    float peak = 0.0f;
    const float smoothing = 1.0f - std::exp(-1.0f / (0.003f * static_cast<float>(rate_)));
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (playing && framesUntilStep_ <= 0.0) {
            for (int track = 0; track < trackCount; ++track)
                if (patterns[track] & (1u << nextStep_)) positions_[track] = 0;
            controls_.currentStep.store(nextStep_, std::memory_order_relaxed);
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
    controls_.peak.store(peak, std::memory_order_relaxed);
}
} // namespace drum
