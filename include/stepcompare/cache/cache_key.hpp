#pragma once

#include <cstdint>
#include <string>

namespace stepcompare::cache {

struct FileIdentity final {
    std::string sha256Hex;
    std::uintmax_t sizeBytes{};
    std::int64_t modifiedUtcNanoseconds{};
};

struct CacheKeyInput final {
    FileIdentity file;
    std::string importConfiguration;
    std::string algorithmVersion;
};

// Produces a collision-safe, locale-independent canonical key. The content
// hash itself is supplied by the filesystem adapter so this module remains
// independent of a platform crypto API.
[[nodiscard]] std::string makeCacheKey(const CacheKeyInput& input);

}  // namespace stepcompare::cache
