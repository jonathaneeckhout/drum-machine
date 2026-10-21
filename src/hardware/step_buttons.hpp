#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <span>

namespace drum {
struct StepButtonConfig {
    bool enabled = false;
    std::filesystem::path device = "/dev/i2c-1";
    std::array<std::uint8_t, 2> addresses{0x20, 0x21};
};

// Register transactions are the boundary between tested button logic and Linux I2C.
class I2cRegisters {
public:
    virtual ~I2cRegisters() = default;
    virtual void write(std::uint8_t address, std::uint8_t reg, std::span<const std::uint8_t> bytes) = 0;
    virtual void read(std::uint8_t address, std::uint8_t reg, std::span<std::uint8_t> bytes) = 0;
};

class LinuxI2cRegisters final : public I2cRegisters {
public:
    explicit LinuxI2cRegisters(std::filesystem::path device);
    ~LinuxI2cRegisters() override;
    LinuxI2cRegisters(const LinuxI2cRegisters&) = delete;
    LinuxI2cRegisters& operator=(const LinuxI2cRegisters&) = delete;
    void write(std::uint8_t address, std::uint8_t reg, std::span<const std::uint8_t> bytes) override;
    void read(std::uint8_t address, std::uint8_t reg, std::span<std::uint8_t> bytes) override;
private:
    std::filesystem::path device_;
    int fd_ = -1;
    void transfer(void* messages, unsigned count);
};

// Call only on the serialized control thread. Each bit returned is one debounced
// press on steps 1..16. Held buttons do not repeat; connection/reconnection primes
// the current levels without generating presses. Each board recovers independently.
class Mcp23017Steps {
public:
    using Clock = std::chrono::steady_clock;
    Mcp23017Steps(I2cRegisters& bus, std::array<std::uint8_t, 2> addresses, std::ostream& log);
    std::uint16_t poll(Clock::time_point now);
private:
    struct Board {
        bool ready = false, reportedFailure = false;
        std::uint8_t stable = 0, candidate = 0;
        std::array<Clock::time_point, 8> changed{};
        Clock::time_point retry{}, verify{};
    };
    I2cRegisters& bus_;
    const std::array<std::uint8_t, 2> addresses_;
    std::ostream& log_;
    std::array<Board, 2> boards_{};
    void configure(std::uint8_t address);
    std::uint8_t pressed(std::uint8_t address);
};
} // namespace drum
