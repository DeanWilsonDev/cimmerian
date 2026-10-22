#pragma once

#include <filesystem>
#include <string>

namespace Cimmerian::Snapshot {

// Writes contents to a temporary file next to path, then renames it over
// path. Another process reading path at the same time sees the old file or
// the new one, never part of either. Creates path's directory if it's
// missing. On failure, returns false, sets *error to the reason and leaves no
// temporary file behind.
bool WriteFileAtomically(
    const std::filesystem::path& path,
    const std::string& contents,
    std::string* error
);

} // namespace Cimmerian::Snapshot
