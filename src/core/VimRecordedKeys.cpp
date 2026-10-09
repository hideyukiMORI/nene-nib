#include "VimRecordedKeys.hpp"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace nenenib::core
{
VimRecordedKeys::VimRecordedKeys(Storage storage) : storage_(std::move(storage)) {}

VimRecordedKeys VimRecordedKeys::from(std::span<const VimKey> keys)
{
    if (keys.empty())
    {
        return VimRecordedKeys(Storage{std::in_place_type<Chunks>});
    }
    Chunks next;
    next.reserve(keys.size() / chunk_capacity +
                 static_cast<std::size_t>(keys.size() % chunk_capacity != 0));
    for (std::size_t at = 0; at < keys.size();)
    {
        const std::size_t count = std::min(chunk_capacity, keys.size() - at);
        const auto part = keys.subspan(at, count);
        next.push_back(std::make_shared<const Chunk>(part.begin(), part.end()));
        at += count;
    }
    return VimRecordedKeys(Storage{std::make_shared<const Chunks>(std::move(next))});
}

const VimRecordedKeys::Chunks &VimRecordedKeys::chunks() const
{
    return std::visit(
        [](const auto &value) -> const Chunks &
        {
            using Alternative = std::decay_t<decltype(value)>;
            static_assert(std::is_same_v<Alternative, Chunks> ||
                          std::is_same_v<Alternative, std::shared_ptr<const Chunks>>);
            if constexpr (std::is_same_v<Alternative, Chunks>)
            {
                return value;
            }
            if constexpr (std::is_same_v<Alternative, std::shared_ptr<const Chunks>>)
            {
                return *value;
            }
        },
        storage_);
}

VimRecordedKeys VimRecordedKeys::appended(const VimKey &key) const
{
    Chunks next = chunks();
    if (!next.empty() && next.back()->size() < chunk_capacity)
    {
        Chunk tail = *next.back();
        tail.push_back(key);
        next.back() = std::make_shared<const Chunk>(std::move(tail));
        return VimRecordedKeys(Storage{std::make_shared<const Chunks>(std::move(next))});
    }
    Chunk tail;
    tail.push_back(key);
    next.push_back(std::make_shared<const Chunk>(std::move(tail)));
    return VimRecordedKeys(Storage{std::make_shared<const Chunks>(std::move(next))});
}

std::vector<VimKey> VimRecordedKeys::owned_keys() const
{
    const Chunks &entries = chunks();
    std::size_t count = 0;
    for (const auto &entry : entries)
    {
        count += entry->size();
    }
    std::vector<VimKey> result;
    result.reserve(count);
    for (const auto &entry : entries)
    {
        result.insert(result.end(), entry->begin(), entry->end());
    }
    return result;
}
} // namespace nenenib::core
