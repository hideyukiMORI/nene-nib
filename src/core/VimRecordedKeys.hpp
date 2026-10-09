#pragma once

#include "VimKey.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <variant>
#include <vector>

namespace nenenib::core
{
// 記録中/確定済み鍵の不変 snapshot。raw 鍵は防御コピーし、共有値の rvalue も copy する（ADR
// 0089）。
class VimRecordedKeys final
{
  public:
    VimRecordedKeys(const VimRecordedKeys &) = default;
    VimRecordedKeys &operator=(const VimRecordedKeys &) = default;

    [[nodiscard]] static VimRecordedKeys from(std::span<const VimKey> keys);
    [[nodiscard]] VimRecordedKeys appended(const VimKey &key) const;
    [[nodiscard]] std::vector<VimKey> owned_keys() const;

  private:
    using Chunk = std::vector<VimKey>;
    using Chunks = std::vector<std::shared_ptr<const Chunk>>;
    using Storage = std::variant<Chunks, std::shared_ptr<const Chunks>>;
    static constexpr std::size_t chunk_capacity = 64;

    explicit VimRecordedKeys(Storage storage);
    [[nodiscard]] const Chunks &chunks() const;
    Storage storage_;
};
} // namespace nenenib::core
