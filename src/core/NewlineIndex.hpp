#pragma once

#include "LineLengths.hpp"
#include "NewlineChunk.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class TextBuffer;

// source を 64 KiB の塊に切り、塊ごとに「塊の先頭までの累積の改行数」と、疎（LF の位置の 16 bit の
// 列）か密（1 byte に 1 bit と 512 byte ごとの累積）の表現を持ち、本文を読まずに行位置を引く
// （ADR 0121 と 2026-10-22 の追補）。塊ごとに行の長さの要約も持ち、窓の最長の行を塊の数に比例して
// 出す（Issue #553）。
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
    // 両端の欠けた塊だけを塊の表現から数える。本文は読まない。
    [[nodiscard]] LineLengths lengths(std::size_t begin, std::size_t end) const noexcept;

  private:
    friend class TextBuffer;
    NewlineIndex() = default;
    void append(std::string_view text);
    // 末尾の塊の local バイト目から part を書く（part は塊の終わりを越えない）。
    void append_part(std::string_view part, std::size_t local);
    // 1 つの塊の中の [begin, end) の要約を塊の表現から数える。読む量は区間に比例する。
    [[nodiscard]] LineLengths scanned(std::size_t begin, std::size_t end) const noexcept;

    // 塊ごとの「塊の先頭までの累積の改行数」・表現・行の長さの要約（末尾の塊は書きかけ）。
    std::vector<std::size_t> counts_;
    std::vector<NewlineChunk> chunks_;
    std::vector<LineLengths> lengths_;
    std::size_t size_bytes_ = 0;
    std::size_t newline_count_ = 0;
};
} // namespace nenenib::core
