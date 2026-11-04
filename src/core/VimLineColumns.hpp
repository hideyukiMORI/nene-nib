#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "VirtualColumn.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace nenenib::core
{
// 矩形の入口（`vim_block_range` と矩形の INSERT・`r`・`p`）と 1 文字戻り（Esc・`h` `X` `dh`）
// が 1 行の桁を問う借用の口（#1198 / #1237 / #1258・ADR 0035 の追補）。バイトは行頭からの数。
// 仮想桁の索引を持つ本文（巨大な行の文書と格子・ADR 0140 の決定 4）は行を写さず、桁は索引の
// 部分和から引き、1 文字の境と code point は問いの位置の周りの窓だけを読む。持たない本文は行を
// 1 回だけ写し、`std::string_view` 版の桁の関数（column_of・byte_at_column ほか）に今のまま渡す
// （ADR 0139 の決定 6・ADR 0144 の決定 2 と同じ分け方）。寿命は呼び出しの間だけで、本文を越えて
// 持たない（VimEditorView と同じ借用）。
class VimLineColumns final
{
  public:
    VimLineColumns(const TextBuffer &text, LineNumber line);

    // 行の内容の最初のバイトの位置と、内容のバイト数（改行を含まない）。
    [[nodiscard]] Offset start() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    // 行の内容が占める桁の数（virtual_width）。
    [[nodiscard]] std::size_t width() const;
    // その桁を含む文字の先頭（byte_at_column）。
    [[nodiscard]] std::size_t byte_at(VirtualColumn column) const;
    // byte の文字が占める最初の桁と最後の桁（column_of・column_end_of）。
    [[nodiscard]] VirtualColumn column_of(std::size_t byte) const;
    [[nodiscard]] VirtualColumn column_end_of(std::size_t byte) const;
    // byte から始まる Vim の 1 文字の終わり（vim_character_end）と、byte から手前へ Vim の 1 文字
    // ずつ count 回戻った頭（vim_character_start を count 回・行頭で止まる）。
    [[nodiscard]] std::size_t character_end(std::size_t byte) const;
    [[nodiscard]] std::size_t character_start(std::size_t byte, std::size_t count) const;
    // byte の code point と、次の code point の先頭（Utf8.hpp の同じ名前の関数）。
    [[nodiscard]] char32_t code_point_at(std::size_t byte) const;
    [[nodiscard]] std::size_t next_code_point(std::size_t byte) const;

  private:
    std::reference_wrapper<const TextBuffer> text_;
    LineNumber line_;
    Offset start_;
    std::size_t size_;
    // 索引を持たない本文の行の写し。持つ本文では空。
    std::optional<std::string> copied_;
};
} // namespace nenenib::core
