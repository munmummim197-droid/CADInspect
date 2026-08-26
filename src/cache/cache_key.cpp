#include <stepcompare/cache/cache_key.hpp>

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace stepcompare::cache {
namespace {

bool isSha256Hex(const std::string_view value) noexcept {
    if (value.size() != 64) {
        return false;
    }
    for (const char character : value) {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

template <typename Integer>
void appendInteger(std::string& output, const Integer value) {
    char buffer[32]{};
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc{}) {
        throw std::runtime_error("Cannot serialize cache-key integer");
    }
    output.append(buffer, result.ptr);
}

void appendField(std::string& output,
                 const std::string_view name,
                 const std::string_view value) {
    output.append(name);
    output.push_back('=');
    appendInteger(output, value.size());
    output.push_back(':');
    output.append(value);
    output.push_back('|');
}

}  // namespace

std::string makeCacheKey(const CacheKeyInput& input) {
    if (!isSha256Hex(input.file.sha256Hex)) {
        throw std::invalid_argument(
            "Cache content hash must be 64 lowercase hexadecimal characters");
    }
    if (input.importConfiguration.empty() || input.algorithmVersion.empty()) {
        throw std::invalid_argument(
            "Cache import configuration and algorithm version cannot be empty");
    }

    std::string result;
    result.reserve(160 + input.importConfiguration.size() +
                   input.algorithmVersion.size());
    appendField(result, "sha256", input.file.sha256Hex);
    result.append("size=");
    appendInteger(result, input.file.sizeBytes);
    result.push_back('|');
    result.append("mtime_ns=");
    appendInteger(result, input.file.modifiedUtcNanoseconds);
    result.push_back('|');
    appendField(result, "import", input.importConfiguration);
    appendField(result, "algorithm", input.algorithmVersion);
    return result;
}

}  // namespace stepcompare::cache
