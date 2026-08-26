#pragma once

#include <stepcompare/cache/cache_key.hpp>

#include <filesystem>
#include <optional>

namespace stepcompare::cache {

// Reads the file once and returns a real SHA-256/size/mtime identity. Failure is
// explicit so callers cannot silently fall back to a filename-only cache key.
[[nodiscard]] std::optional<FileIdentity> computeFileIdentity(
    const std::filesystem::path& path) noexcept;

}  // namespace stepcompare::cache
