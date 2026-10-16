#pragma once

#include "LineLengths.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class TextBuffer;

// source の各byteに1 bit。512 byteごとの累積数から本文を読まずに行位置を引く（ADR 0121）。
// 512 byteの塊ごとに行の長さの要約も持ち、窓の最長の行を塊の数に比例して出す（Issue #553）。
class NewlineIndex final
{
  public:
    [[nodiscard]] static NewlineIndex empty();
    [[nodiscard]] static NewlineIndex from_text(std::string_view text);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t count_before(std::size_t offset) const noexcept;
    // ordinal < size()。piece の窓内だけを読む。
    [[nodiscard]] Offset at(std::size_t ordinal) const noexcept;
    // source の [begin, end) の行の長さの要約（end <= size_bytes）。窓に収まる塊は要約を畳み、
    // 両端の欠けた塊だけを bit から数える。本文は読まない。
    [[nodiscard]] LineLengths lengths(std::size_t begin, std::size_t end) const noexcept;

  private:
    friend class TextBuffer;
    NewlineIndex() = default;
    void append(std::string_view text);
    // [begin, end) の要約を bit から数える。読む word は区間に比例する。
    [[nodiscard]] LineLengths scanned(std::size_t begin, std::size_t end) const noexcept;

    std::vector<std::uint64_t> words_;
    std::vector<std::size_t> counts_;
    // 書き終えた塊ごとの行の長さの要約と、書きかけの末尾の塊の要約。
    std::vector<LineLengths> blocks_;
    LineLengths pending_;
    std::size_t size_bytes_ = 0;
    std::size_t newline_count_ = 0;
};
} // namespace nenenib::core
