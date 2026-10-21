#pragma once
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>

namespace drum {
class Instrument;
std::filesystem::path defaultControlSocket();
// One control thread executes all requests; audio continues on its own thread.
int runService(Instrument& instrument, const std::filesystem::path& socket,
               const std::function<bool()>& audioFailed);
int runClient(const std::filesystem::path& socket, std::istream& input,
              std::ostream& output, const std::string& command = {});
} // namespace drum
