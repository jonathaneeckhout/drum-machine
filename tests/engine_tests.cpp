#include "engine.hpp"

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
void clear(drum::Controls& controls) {
    for (auto& pattern : controls.patterns) pattern.store(0);
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
        drum::Controls controls;
        clear(controls);
        controls.patterns[0].store(0xffff);
        controls.bpm.store(123.0); // Fractional samples per step exercises phase accumulation.
        controls.playing.store(true);
        drum::Engine engine(controls, impulses());
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
        drum::Controls controls;
        clear(controls);
        controls.patterns[0].store(0x8001);
        controls.playing.store(true);
        drum::Engine engine(controls, impulses());
        std::vector<float> output(96001 * 2);
        engine.render(output.data(), 96001);
        check(hits(output) == std::vector<std::size_t>({0, 90000, 96000}),
              "first and last steps map to correct pattern bits and wrap");
        controls.playing.store(false);
        engine.render(output.data(), 128);
        check(controls.currentStep.load() == -1, "stop clears the playhead");
        check(hits(std::vector<float>(output.begin(), output.begin() + 256)).empty(),
              "stopped transport does not retrigger drums");
        controls.playing.store(true);
        engine.render(output.data(), 1);
        check(output[0] > 0.0f && controls.currentStep.load() == 0,
              "restart begins immediately at step one");
    }
    {
        drum::Controls controls;
        clear(controls);
        controls.patterns[0].store(0xffff);
        controls.playing.store(true);
        drum::Engine engine(controls, impulses());
        std::vector<float> output(6000 * 2);
        engine.render(output.data(), 3000); // Half a step at 120 BPM.
        controls.bpm.store(240);
        engine.render(output.data(), 3001);
        check(hits(std::vector<float>(output.begin(), output.begin() + 6002)) ==
                  std::vector<std::size_t>({1500}),
              "tempo changes preserve fractional progress through the current step");
    }
    {
        drum::Controls controls;
        clear(controls);
        controls.previewRequests.store(0xf);
        drum::Engine engine(controls, impulses());
        float output[4]{};
        engine.render(output, 2);
        check(output[0] > output[1] && output[0] < 1.0f,
              "four simultaneous previews mix stereo safely while stopped");
        check(controls.previewRequests.load() == 0 && output[2] == 0,
              "preview is consumed once");
        check(controls.currentStep.load() == -1, "preview does not start the sequencer");
    }
    {
        drum::Controls controls;
        clear(controls);
        controls.patterns[0].store(1);
        controls.volumes[0].store(std::numeric_limits<float>::infinity());
        controls.bpm.store(std::numeric_limits<double>::quiet_NaN());
        controls.playing.store(true);
        drum::Engine engine(controls, impulses());
        float output[64];
        engine.render(output, 32);
        for (float value : output) check(value == 0.0f, "invalid volume is sanitized before mixing");
        check(controls.currentStep.load() == 0, "invalid tempo uses a safe fallback");
    }
    {
        drum::Controls controls;
        controls.bpm.store(std::numeric_limits<double>::quiet_NaN());
        controls.volumes[0].store(std::numeric_limits<float>::infinity());
        controls.playing.store(true);
        drum::Engine engine(controls, {});
        float output[64];
        engine.render(output, 32);
        for (float value : output) check(value == 0.0f, "empty samples and invalid controls remain safe");
    }
    std::cout << "All engine tests passed.\n";
}
