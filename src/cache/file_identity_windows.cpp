#include <stepcompare/cache/file_identity.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace stepcompare::cache {
namespace {

class AlgorithmHandle final {
public:
    ~AlgorithmHandle() {
        if (value != nullptr) {
            BCryptCloseAlgorithmProvider(value, 0);
        }
    }
    BCRYPT_ALG_HANDLE value{};
};

class HashHandle final {
public:
    ~HashHandle() {
        if (value != nullptr) {
            BCryptDestroyHash(value);
        }
    }
    BCRYPT_HASH_HANDLE value{};
};

bool property(BCRYPT_HANDLE handle, const wchar_t* name, ULONG& output) {
    ULONG written{};
    return BCRYPT_SUCCESS(BCryptGetProperty(
               handle,
               name,
               reinterpret_cast<PUCHAR>(&output),
               sizeof(output),
               &written,
               0)) &&
           written == sizeof(output);
}

std::string toHex(const std::vector<unsigned char>& digest) {
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string result;
    result.resize(digest.size() * 2);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2] = digits[digest[index] >> 4U];
        result[index * 2 + 1] = digits[digest[index] & 0x0fU];
    }
    return result;
}

}  // namespace

std::optional<FileIdentity> computeFileIdentity(
    const std::filesystem::path& path) noexcept {
    try {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error) {
            return std::nullopt;
        }
        const auto modified = std::filesystem::last_write_time(path, error);
        if (error) {
            return std::nullopt;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return std::nullopt;
        }

        AlgorithmHandle algorithm;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
                &algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
            return std::nullopt;
        }
        ULONG objectBytes{};
        ULONG digestBytes{};
        if (!property(algorithm.value, BCRYPT_OBJECT_LENGTH, objectBytes) ||
            !property(algorithm.value, BCRYPT_HASH_LENGTH, digestBytes)) {
            return std::nullopt;
        }

        std::vector<unsigned char> object(objectBytes);
        HashHandle hash;
        if (!BCRYPT_SUCCESS(BCryptCreateHash(
                algorithm.value,
                &hash.value,
                object.data(),
                static_cast<ULONG>(object.size()),
                nullptr,
                0,
                0))) {
            return std::nullopt;
        }

        std::vector<char> buffer(1024 * 1024);
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count > 0 && !BCRYPT_SUCCESS(BCryptHashData(
                                 hash.value,
                                 reinterpret_cast<PUCHAR>(buffer.data()),
                                 static_cast<ULONG>(count),
                                 0))) {
                return std::nullopt;
            }
        }
        if (!input.eof()) {
            return std::nullopt;
        }

        std::vector<unsigned char> digest(digestBytes);
        if (!BCRYPT_SUCCESS(BCryptFinishHash(
                hash.value,
                digest.data(),
                static_cast<ULONG>(digest.size()),
                0))) {
            return std::nullopt;
        }

        return FileIdentity{
            .sha256Hex = toHex(digest),
            .sizeBytes = size,
            .modifiedUtcNanoseconds = modified.time_since_epoch().count(),
        };
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace stepcompare::cache
