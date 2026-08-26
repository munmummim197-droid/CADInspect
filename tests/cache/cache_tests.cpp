#include <stepcompare/cache/cache_key.hpp>
#include <stepcompare/cache/file_identity.hpp>
#include <stepcompare/cache/memory_budget_cache.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void cacheKeyContract() {
    using namespace stepcompare::cache;
    CacheKeyInput input{
        .file = {.sha256Hex =
                     "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                 .sizeBytes = 1'234'567,
                 .modifiedUtcNanoseconds = 42},
        .importConfiguration = "xcaf;unit=mm;names=1;colors=1",
        .algorithmVersion = "dev-v1-fast-1",
    };

    const auto key = makeCacheKey(input);
    expect(key.find(input.file.sha256Hex) != std::string::npos,
           "cache key must include the content hash");
    expect(key.find("1234567") != std::string::npos,
           "cache key must include file size");
    expect(key.find("42") != std::string::npos,
           "cache key must include modification time");
    expect(key.find(input.importConfiguration) != std::string::npos,
           "cache key must include import configuration");
    expect(key.find(input.algorithmVersion) != std::string::npos,
           "cache key must include algorithm version");

    auto changed = input;
    changed.algorithmVersion = "dev-v1-fast-2";
    expect(makeCacheKey(changed) != key,
           "algorithm changes must invalidate the cache");
}

void memoryBudgetContract() {
    using stepcompare::cache::MemoryBudgetCache;
    MemoryBudgetCache<std::string> cache{10};
    expect(cache.put("a", "alpha", 6), "first entry must fit");
    expect(cache.put("b", "beta", 4), "second entry must fill the budget");
    expect(cache.get("a").value_or("") == "alpha", "cache hit must return value");
    expect(cache.put("c", "gamma", 4), "new entry must evict under pressure");
    expect(!cache.get("b"), "least recently used entry must be evicted");
    expect(cache.get("a").value_or("") == "alpha",
           "recently used entry must remain resident");
    expect(!cache.put("oversized", "x", 11),
           "entry larger than total budget must be rejected");
    expect(cache.usedBytes() <= cache.budgetBytes(),
           "cache must never exceed its memory budget");
    expect(cache.statistics().hits >= 2 && cache.statistics().misses >= 1 &&
               cache.statistics().evictions >= 1,
           "cache must expose hit/miss/eviction diagnostics");
}

void realUnicodeFileIdentityContract() {
    namespace fs = std::filesystem;
    const auto directory = fs::current_path() / fs::path(u8"Bộ nhớ đệm");
    const auto path = directory / fs::path(u8"Chi tiết 01.step");
    fs::create_directories(directory);
    {
        std::ofstream output(path, std::ios::binary);
        output << "abc";
    }

    const auto identity = stepcompare::cache::computeFileIdentity(path);
    expect(identity.has_value(), "Unicode file identity must be computed");
    if (identity) {
        expect(identity->sha256Hex ==
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               "file hash must be the real SHA-256 digest");
        expect(identity->sizeBytes == 3, "file size must come from the filesystem");
    }
    fs::remove_all(directory);
}

}  // namespace

int main() {
    cacheKeyContract();
    memoryBudgetContract();
    realUnicodeFileIdentityContract();
    if (failures != 0) {
        return EXIT_FAILURE;
    }
    std::cout << "All cache tests passed\n";
    return EXIT_SUCCESS;
}
