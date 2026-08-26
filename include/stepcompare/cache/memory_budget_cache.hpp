#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace stepcompare::cache {

struct CacheStatistics final {
    std::uint64_t hits{};
    std::uint64_t misses{};
    std::uint64_t evictions{};
};

template <typename Value>
class MemoryBudgetCache final {
public:
    explicit MemoryBudgetCache(const std::size_t budgetBytes)
        : budgetBytes_(budgetBytes) {}

    [[nodiscard]] bool put(std::string key,
                           Value value,
                           const std::size_t estimatedBytes) {
        std::scoped_lock lock(mutex_);
        if (estimatedBytes > budgetBytes_) {
            return false;
        }
        if (const auto found = index_.find(key); found != index_.end()) {
            usedBytes_ -= found->second->estimatedBytes;
            entries_.erase(found->second);
            index_.erase(found);
        }
        while (usedBytes_ + estimatedBytes > budgetBytes_ && !entries_.empty()) {
            auto oldest = std::prev(entries_.end());
            usedBytes_ -= oldest->estimatedBytes;
            index_.erase(oldest->key);
            entries_.erase(oldest);
            ++statistics_.evictions;
        }
        entries_.push_front(
            Entry{std::move(key), std::move(value), estimatedBytes});
        usedBytes_ += estimatedBytes;
        index_.emplace(entries_.front().key, entries_.begin());
        return true;
    }

    [[nodiscard]] std::optional<Value> get(const std::string& key) {
        std::scoped_lock lock(mutex_);
        const auto found = index_.find(key);
        if (found == index_.end()) {
            ++statistics_.misses;
            return std::nullopt;
        }
        entries_.splice(entries_.begin(), entries_, found->second);
        ++statistics_.hits;
        return entries_.front().value;
    }

    void clear() noexcept {
        std::scoped_lock lock(mutex_);
        entries_.clear();
        index_.clear();
        usedBytes_ = 0;
    }

    [[nodiscard]] std::size_t budgetBytes() const noexcept {
        return budgetBytes_;
    }

    [[nodiscard]] std::size_t usedBytes() const noexcept {
        std::scoped_lock lock(mutex_);
        return usedBytes_;
    }

    [[nodiscard]] CacheStatistics statistics() const noexcept {
        std::scoped_lock lock(mutex_);
        return statistics_;
    }

private:
    struct Entry final {
        std::string key;
        Value value;
        std::size_t estimatedBytes{};
    };

    using EntryList = std::list<Entry>;
    using EntryIterator = typename EntryList::iterator;

    const std::size_t budgetBytes_;
    mutable std::mutex mutex_;
    std::size_t usedBytes_{};
    CacheStatistics statistics_{};
    EntryList entries_;
    std::unordered_map<std::string, EntryIterator> index_;
};

}  // namespace stepcompare::cache
