#pragma once

#include <memory>
#include <string>

namespace drum {
class Instrument;

// Owns SDL audio initialization, the device, and its callback. Must be destroyed
// before the Instrument. Does not initialize video or own frontend resources.
class AudioOutput {
public:
    explicit AudioOutput(Instrument& instrument, bool dummyDevice = false);
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    bool failed() const noexcept;
    std::string name() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace drum
