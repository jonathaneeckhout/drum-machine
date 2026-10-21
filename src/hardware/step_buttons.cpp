#include "step_buttons.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <ostream>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace drum {
using namespace std::chrono_literals;
LinuxI2cRegisters::LinuxI2cRegisters(std::filesystem::path device) : device_(std::move(device)) {}
LinuxI2cRegisters::~LinuxI2cRegisters() { if (fd_ >= 0) ::close(fd_); }

void LinuxI2cRegisters::transfer(void* messages, unsigned count) {
    if (fd_ < 0) fd_ = ::open(device_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) throw std::runtime_error(device_.string() + ": " + std::strerror(errno));
    i2c_rdwr_ioctl_data transaction{static_cast<i2c_msg*>(messages), count};
    int result;
    do { result = ::ioctl(fd_, I2C_RDWR, &transaction); } while (result < 0 && errno == EINTR);
    if (result != static_cast<int>(count)) {
        const std::string reason = result < 0 ? std::strerror(errno) : "incomplete I2C transaction";
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error(device_.string() + ": " + reason);
    }
}

void LinuxI2cRegisters::write(std::uint8_t address, std::uint8_t reg, std::span<const std::uint8_t> bytes) {
    if (bytes.size() > 2) throw std::runtime_error("I2C register write too large");
    std::array<std::uint8_t, 3> buffer{reg, 0, 0};
    std::copy(bytes.begin(), bytes.end(), buffer.begin() + 1);
    i2c_msg message{address, 0, static_cast<__u16>(bytes.size() + 1), buffer.data()};
    transfer(&message, 1);
}

void LinuxI2cRegisters::read(std::uint8_t address, std::uint8_t reg, std::span<std::uint8_t> bytes) {
    if (bytes.size() > 2) throw std::runtime_error("I2C register read too large");
    std::array<i2c_msg, 2> messages{{{address, 0, 1, &reg},
        {address, I2C_M_RD, static_cast<__u16>(bytes.size()), bytes.data()}}};
    transfer(messages.data(), messages.size());
}

Mcp23017Steps::Mcp23017Steps(I2cRegisters& bus, std::array<std::uint8_t, 2> addresses, std::ostream& log)
    : bus_(bus), addresses_(addresses), log_(log) {
    if (addresses[0] < 0x20 || addresses[0] > 0x27 || addresses[1] < 0x20 || addresses[1] > 0x27 ||
        addresses[0] == addresses[1]) throw std::runtime_error("Step boards need distinct addresses in 0x20..0x27");
}

void Mcp23017Steps::configure(std::uint8_t address) {
    // Power-on register layout: BANK=0. This driver owns both expanders.
    const std::array<std::uint8_t, 1> zero{0};
    const std::array<std::uint8_t, 2> zeros{0, 0}, inputs{0x7f, 0x7f};
    bus_.write(address, 0x0a, zero);   // IOCON: sequential register access enabled
    bus_.write(address, 0x04, zeros);  // Disable interrupt-on-change (polling only)
    bus_.write(address, 0x02, zeros);  // Normal input polarity
    bus_.write(address, 0x14, zeros);  // Drive unused, output-only GPA7/GPB7 low
    bus_.write(address, 0x00, inputs); // Safe pins inputs; GPA7/GPB7 outputs
    bus_.write(address, 0x0c, inputs); // Internal pull-ups on safe input pins
}

std::uint8_t Mcp23017Steps::pressed(std::uint8_t address) {
    std::array<std::uint8_t, 2> gpio{};
    bus_.read(address, 0x12, gpio);
    // Eight buttons per board: GPA0..GPA6, then GPB0. Active low.
    return static_cast<std::uint8_t>((~gpio[0] & 0x7f) | ((~gpio[1] & 1) << 7));
}

std::uint16_t Mcp23017Steps::poll(Clock::time_point now) {
    std::uint16_t events = 0;
    for (unsigned index = 0; index < boards_.size(); ++index) {
        auto& board = boards_[index];
        const auto address = addresses_[index];
        if (!board.ready && now < board.retry) continue;
        try {
            if (!board.ready) {
                configure(address);
                board.stable = board.candidate = pressed(address);
                board.changed.fill(now);
                board.ready = true;
                board.reportedFailure = false;
                board.verify = now + 1s;
                log_ << "Step buttons: board 0x" << std::hex << unsigned(address) << std::dec << " connected\n";
                continue;
            }
            if (now >= board.verify) {
                std::array<std::uint8_t, 2> direction{}, pullups{};
                bus_.read(address, 0x00, direction);
                bus_.read(address, 0x0c, pullups);
                if (direction != std::array<std::uint8_t, 2>{0x7f, 0x7f} || pullups != direction)
                    throw std::runtime_error("board reset or configuration changed");
                board.verify = now + 1s;
            }
            const auto current = pressed(address);
            for (unsigned bit = 0; bit < 8; ++bit) {
                const auto mask = static_cast<std::uint8_t>(1u << bit);
                if ((current & mask) != (board.candidate & mask)) {
                    board.candidate ^= mask;
                    board.changed[bit] = now;
                } else if ((current & mask) != (board.stable & mask) && now - board.changed[bit] >= 20ms) {
                    board.stable ^= mask;
                    if (current & mask) events |= static_cast<std::uint16_t>(1u << (index * 8 + bit));
                }
            }
        } catch (const std::exception& error) {
            if (!board.reportedFailure)
                log_ << "Step buttons: board 0x" << std::hex << unsigned(address) << std::dec << ": "
                     << error.what() << "; terminal/audio remain available, retrying every second\n";
            board.reportedFailure = true;
            board.ready = false;
            board.retry = now + 1s;
        }
    }
    return events;
}
} // namespace drum
