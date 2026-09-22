#pragma once

#include "DisplayWidth.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
class TextBuffer;

// 仮想桁（Vim の virtcol・1 始まり）。行頭からの表示幅で数えるので、code point で数える
// `Column` とは別の型にして混ぜられなくする（CPP-001 / ADR 0034 の決定 2）。`tabstop` は 8 固定で、
// 描画が決める画素の桁とは無関係な、Vim の意味論のためだけの値である。
struct VirtualColumn
{
    std::size_t value;
};

[[nodiscard]] constexpr bool operator==(VirtualColumn left, VirtualColumn right) noexcept
{
    return left.value == right.value;
}

[[nodiscard]] constexpr bool operator<(VirtualColumn left, VirtualColumn right) noexcept
{
    return left.value < right.value;
}

// 1 つの code point の表示幅（表は DisplayWidthRange.hpp・Tab はここに来ない）。
[[nodiscard]] DisplayWidth display_width(char32_t value) noexcept;

// `at` にある文字が占める最初の桁と最後の桁。`at` は code point の先頭でなければならない。
// 桁を増やさない文字（結合文字）はどちらも直前の文字と同じ桁になる。
[[nodiscard]] VirtualColumn virtual_column(const TextBuffer &text, Offset at);
[[nodiscard]] VirtualColumn virtual_column_end(const TextBuffer &text, Offset at);

// Vim がキャレットを描く桁＝ curswant が覚える桁。Tab だけは最後の桁で、全角・制御文字・
// 書式用文字は最初の桁（Vim 9.1 で実測・ADR 0034 の決定 3）。
[[nodiscard]] VirtualColumn caret_virtual_column(const TextBuffer &text, Offset at);

// 行頭に置いた文字列が占める桁の数（Tab は tabstop で止まる）。行の長さと、矩形レジスタの
// 1 行が覆う幅をここで数える（ADR 0035 の決定 5）。
[[nodiscard]] std::size_t virtual_width(std::string_view text) noexcept;

// 行の文字列の中だけで数える同じ 3 つ。矩形は 1 行につき何度も桁を引くので、行を取り直さずに
// 済むこちらを使う（上の TextBuffer 版もこの 3 本の上に載っている・ARC-001）。
[[nodiscard]] VirtualColumn column_of(std::string_view line, std::size_t byte) noexcept;
[[nodiscard]] VirtualColumn column_end_of(std::string_view line, std::size_t byte) noexcept;
[[nodiscard]] std::size_t byte_at_column(std::string_view line, VirtualColumn column) noexcept;

// 逆引き。その桁を含む文字の先頭を返す。行がそれより短ければ行の内容の終わり（Vim が NUL を
// 置く桁）で、空行なら行頭。欲しい列の着地はこの 1 本だけを通る（ADR 0034 の決定 2）。
[[nodiscard]] Offset offset_at_virtual_column(const TextBuffer &text, LineNumber line,
                                              VirtualColumn column);
} // namespace nenenib::core
