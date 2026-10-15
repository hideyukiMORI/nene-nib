#pragma once

#include "Offset.hpp"
#include "simd/AsciiNewlineScan.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class TextBuffer;

// source の各byteに1 bit。512 byteごとの累積数から本文を読まずに行位置を引く（ADR 0121）。
class NewlineIndex final
{
  public:
    [[nodiscard]] static NewlineIndex empty();
    [[nodiscard]] static NewlineIndex from_text(std::string_view text);
    // 合成ルートが選んだ走査で LF を数える（ADR 0006 の決定 2）。索引の形は同じ。
    [[nodiscard]] static NewlineIndex from_text(std::string_view text, simd::AsciiNewlineScan scan);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t count_before(std::size_t offset) const noexcept;
    // ordinal < size()。piece の窓内だけを読む。
    [[nodiscard]] Offset at(std::size_t ordinal) const noexcept;

  private:
    friend class TextBuffer;
    NewlineIndex() = default;
    void append(std::string_view text);
    void append(std::string_view text, simd::AsciiNewlineScan scan);

    std::vector<std::uint64_t> words_;
    std::vector<std::size_t> counts_;
    std::size_t size_bytes_ = 0;
    std::size_t newline_count_ = 0;
};
} // namespace nenenib::core
