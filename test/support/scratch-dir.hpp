#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace CimmerianTestSupport {

// A directory of its own under the system temp directory, removed with
// everything in it when this goes out of scope.
struct ScratchDir {
  explicit ScratchDir(const std::string& prefix = "cimmerian-scratch-")
  {
    std::random_device random;
    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    do {
      this->path = temp / (prefix + std::to_string(random()));
    } while (!std::filesystem::create_directory(this->path));
  }

  ~ScratchDir()
  {
    std::error_code ignored;
    std::filesystem::remove_all(this->path, ignored);
  }

  ScratchDir(const ScratchDir&) = delete;
  ScratchDir& operator=(const ScratchDir&) = delete;

  std::filesystem::path path;
};

} // namespace CimmerianTestSupport
