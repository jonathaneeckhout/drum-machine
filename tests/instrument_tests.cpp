#include "instrument.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::array<drum::Sample, 4> impulses() {
    std::array<drum::Sample, 4> samples;
    for (auto& sample : samples) sample.pcm = {1.0f, 0.5f};
    return samples;
}
void pattern(drum::Instrument& instrument, int track, unsigned mask) {
    for (int step = 0; step < drum::stepCount; ++step)
        if (mask & (1u << step)) instrument.toggleStep(track, step);
}
std::vector<std::size_t> hits(const std::vector<float>& pcm) {
    std::vector<std::size_t> result;
    for (std::size_t frame = 0; frame < pcm.size() / 2; ++frame)
        if (std::abs(pcm[frame * 2]) > 0.001f) result.push_back(frame);
    return result;
}
}

int main() {
    {
        drum::Instrument engine(impulses());
        engine.clearPattern();
        pattern(engine, 0, 0xffff);
        engine.setTempo(123.0); // Fractional samples per step exercises phase accumulation.
        engine.play();
        std::vector<float> output(drum::sampleRate * 8 * 2);
        for (std::size_t offset = 0; offset < output.size() / 2;) {
            const auto count = std::min<std::size_t>(317, output.size() / 2 - offset);
            engine.render(output.data() + offset * 2, count);
            offset += count;
        }
        const auto triggers = hits(output);
        const double interval = drum::sampleRate * 60.0 / (123.0 * 4.0);
        check(triggers.size() > 60, "sequence loops across multiple bars");
        for (std::size_t i = 0; i < triggers.size(); ++i)
            check(std::abs(static_cast<double>(triggers[i]) - interval * i) <= 1.01,
                  "triggers stay within one sample across irregular callback boundaries");
    }
    {
        drum::Instrument engine(impulses());
        engine.clearPattern();
        pattern(engine, 0, 0x8001);
        engine.play();
        std::vector<float> output(96001 * 2);
        engine.render(output.data(), 96001);
        check(hits(output) == std::vector<std::size_t>({0, 90000, 96000}),
              "first and last steps map to correct pattern bits and wrap");
        engine.stop();
        engine.render(output.data(), 128);
        check(engine.status().currentStep == -1, "stop clears the playhead");
        check(hits(std::vector<float>(output.begin(), output.begin() + 256)).empty(),
              "stopped transport does not retrigger drums");
        engine.play();
        engine.render(output.data(), 1);
        check(output[0] > 0.0f && engine.status().currentStep == 0,
              "restart begins immediately at step one");
    }
    {
        drum::Instrument engine(impulses());
        engine.clearPattern();
        pattern(engine, 0, 0xffff);
        engine.play();
        std::vector<float> output(6000 * 2);
        engine.render(output.data(), 3000); // Half a step at 120 BPM.
        engine.setTempo(240);
        engine.render(output.data(), 3001);
        check(hits(std::vector<float>(output.begin(), output.begin() + 6002)) ==
                  std::vector<std::size_t>({1500}),
              "tempo changes preserve fractional progress through the current step");
    }
    {
        drum::Instrument engine(impulses());
        engine.clearPattern();
        for (int track = 0; track < 4; ++track) engine.preview(track);
        float output[4]{};
        engine.render(output, 2);
        check(output[0] > output[1] && output[0] < 1.0f,
              "four simultaneous previews mix stereo safely while stopped");
        check(output[2] == 0,
              "preview is consumed once");
        check(engine.status().currentStep == -1, "preview does not start the sequencer");
    }
    {
        drum::Instrument engine(impulses());
        engine.clearPattern();
        engine.setVolume(0, std::numeric_limits<float>::infinity());
        engine.setTempo(std::numeric_limits<double>::quiet_NaN());
        check(engine.status().tracks[0].volume == 0 && engine.status().tempo == 120,
              "invalid numeric commands are sanitized at the interface");
        engine.setVolume(0, 0.42f);
        engine.toggleMute(0);
        check(engine.status().tracks[0].muted, "mute is instrument behavior");
        engine.toggleMute(0);
        check(std::abs(engine.status().tracks[0].volume - 0.42f) < 0.0001f, "unmute restores the previous volume");
        engine.setTempo(160);
        engine.loadDemo();
        check(engine.status().tempo == 160 && engine.status().tracks[0].steps[0], "demo restores steps while preserving controls");
        const auto before = engine.status();
        engine.toggleStep(-1, 0); engine.toggleStep(0, 16); engine.setVolume(4, 1); engine.toggleMute(-1); engine.preview(4);
        check(engine.status().tracks[0].steps == before.tracks[0].steps, "invalid indices do not modify the pattern");
    }
    {
        drum::Instrument engine({});
        engine.play();
        float output[64];
        engine.render(output, 32);
        for (float value : output) check(value == 0.0f, "empty samples remain silent");
    }
    std::cout << "All instrument tests passed.\n";
}
