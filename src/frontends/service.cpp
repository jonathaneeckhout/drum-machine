#include "service.hpp"
#include "terminal.hpp"
#include "instrument.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace drum {
namespace {
constexpr std::size_t maxCommand = 4096;
volatile std::sig_atomic_t stopping = 0;
void stopService(int) { stopping = 1; }

class Descriptor {
public:
    explicit Descriptor(int fd) : fd_(fd) {
        if (fd < 0) throw std::runtime_error(std::string("Control socket: ") + std::strerror(errno));
    }
    ~Descriptor() { ::close(fd_); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};

sockaddr_un address(const std::filesystem::path& path) {
    sockaddr_un result{};
    result.sun_family = AF_UNIX;
    const auto name = path.string();
    if (name.empty() || name.size() >= sizeof(result.sun_path))
        throw std::runtime_error("Control socket path is empty or too long");
    std::memcpy(result.sun_path, name.c_str(), name.size() + 1);
    return result;
}

void timeout(int fd) {
    const timeval value{2, 0};
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) < 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value)) < 0)
        throw std::runtime_error("Cannot set control socket timeout");
}

bool sendAll(int fd, std::string_view data) {
    while (!data.empty()) {
        const auto count = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR && !stopping) continue;
        if (count <= 0) return false;
        data.remove_prefix(static_cast<std::size_t>(count));
    }
    return true;
}

struct Signals {
    struct sigaction oldTerm{}, oldInt{};
    Signals() {
        stopping = 0;
        struct sigaction action{};
        action.sa_handler = stopService;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGTERM, &action, &oldTerm) < 0)
            throw std::runtime_error("Cannot install SIGTERM handler");
        if (sigaction(SIGINT, &action, &oldInt) < 0) {
            sigaction(SIGTERM, &oldTerm, nullptr);
            throw std::runtime_error("Cannot install SIGINT handler");
        }
    }
    ~Signals() {
        sigaction(SIGTERM, &oldTerm, nullptr);
        sigaction(SIGINT, &oldInt, nullptr);
    }
};

std::string request(const std::filesystem::path& path, const std::string& command) {
    if (command.size() > maxCommand || command.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("Commands must be a single line of at most 4096 bytes");
    Descriptor connection(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    timeout(connection.get());
    const auto target = address(path);
    if (::connect(connection.get(), reinterpret_cast<const sockaddr*>(&target), sizeof(target)) < 0)
        throw std::runtime_error("Cannot connect to " + path.string() + ": " + std::strerror(errno));
    if (!sendAll(connection.get(), command + '\n'))
        throw std::runtime_error("Cannot send control command");
    std::string response;
    char buffer[1024];
    for (;;) {
        const auto count = ::recv(connection.get(), buffer, sizeof(buffer), 0);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw std::runtime_error("Control service did not respond");
        if (count == 0) break;
        response.append(buffer, static_cast<std::size_t>(count));
        if (response.size() > 65536) throw std::runtime_error("Control response too large");
    }
    if (response.empty()) throw std::runtime_error("Control service closed without a response");
    return response;
}
}

std::filesystem::path defaultControlSocket() {
    if (const char* configured = std::getenv("DRUM_MACHINE_SOCKET")) return configured;
    return "/run/drum-machine/control.sock";
}

int runService(Instrument& instrument, const std::filesystem::path& path,
               const std::function<bool()>& audioFailed) {
    const auto target = address(path);
    Descriptor listener(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    // Never unlink an existing socket: a second service must not steal a live endpoint.
    // systemd removes the private runtime directory on service shutdown/restart.
    if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&target), sizeof(target)) < 0)
        throw std::runtime_error("Cannot bind " + path.string() + ": " + std::strerror(errno));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { ::unlink(path.c_str()); }
    } cleanup{path};
    if (::chmod(path.c_str(), 0600) < 0 || ::listen(listener.get(), 8) < 0)
        throw std::runtime_error("Cannot prepare control socket");
    Signals signals;
    std::cout << "Control service listening on " << path << std::endl;
    while (!stopping && !audioFailed()) {
        pollfd ready{listener.get(), POLLIN, 0};
        const int result = ::poll(&ready, 1, 100);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) throw std::runtime_error("Control socket poll failed");
        if (result == 0) continue;
        if (!(ready.revents & POLLIN)) throw std::runtime_error("Control listener closed unexpectedly");
        const int accepted = ::accept4(listener.get(), nullptr, nullptr, SOCK_CLOEXEC);
        if (accepted < 0 && errno == EINTR) continue;
        Descriptor client(accepted);
        timeout(client.get());
        std::string line;
        char buffer[256];
        bool complete = false;
        while (!stopping && line.size() <= maxCommand) {
            const auto count = ::recv(client.get(), buffer, sizeof(buffer), 0);
            if (count < 0 && errno == EINTR && !stopping) continue;
            if (count <= 0) break;
            line.append(buffer, static_cast<std::size_t>(count));
            if (line.find('\n') != std::string::npos) { complete = true; break; }
        }
        std::ostringstream response;
        const auto newline = line.find('\n');
        if (!complete || newline > maxCommand) response << "Invalid or incomplete control request.\n";
        else if (!executeTerminalCommand(instrument, std::string_view(line).substr(0, newline), response))
            response << "quit disconnects the client; use stop to stop playback.\n";
        if (response.str().empty()) response << "ok\n";
        sendAll(client.get(), response.str());
    }
    instrument.stop();
    return 0;
}

int runClient(const std::filesystem::path& path, std::istream& input,
              std::ostream& output, const std::string& command) {
    if (!command.empty()) {
        const auto response = request(path, command);
        output << response;
        return response.starts_with("Invalid") ? 1 : 0;
    }
    output << "drum-machine / connected terminal\n"
              "Type help for commands; quit or EOF disconnects without stopping playback.\n";
    std::string line;
    while (output << "> " << std::flush, std::getline(input, line)) {
        const auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        const auto end = line.find_last_not_of(" \t\r");
        const auto trimmed = line.substr(start, end - start + 1);
        if (trimmed == "quit") break;
        output << request(path, trimmed);
    }
    return 0;
}
} // namespace drum
