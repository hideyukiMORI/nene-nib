#pragma once

#include "LspPositionEncoding.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// LSP の Position（0 始まりの行と、行頭からの `positionEncoding` の単位の数・ADR 0160 の決定 5）。
// 行と桁は互いに独立して妥当なので公開 aggregate。比較は非メンバー（CPP-003）。
struct LspPosition
{
    std::size_t line;
    std::size_t character;
};

[[nodiscard]] constexpr bool operator==(const LspPosition &left, const LspPosition &right) noexcept
{
    return left.line == right.line && left.character == right.character;
}

// Position → 本文のバイト位置の唯一の写し（ADR 0160 の決定 5）。行は TextBuffer の行（改行は
// TextBuffer の模型に従う）で、本文の外の行は値なし。桁が行の内容の外なら行末（改行の前）に
// 切り詰める。コードポイントの途中（UTF-8 のバイトの途中・UTF-16 の代理対の途中）に落ちた桁は
// 後ろの境目へ寄せる。UTF-16 の単位は Utf16 の utf16_length の 1 本で数える。
[[nodiscard]] std::optional<Offset> lsp_offset_of(const TextBuffer &buffer, LspPosition position,
                                                  LspPositionEncoding encoding);

// 本文のバイト位置 → Position（lsp_offset_of の逆）。本文の外は末尾に、行の改行の中（CRLF の
// `\r` の後ろ）は行末に切り詰め、コードポイントの途中は後ろの境目へ寄せる。
[[nodiscard]] LspPosition lsp_position_of(const TextBuffer &buffer, Offset at,
                                          LspPositionEncoding encoding);
} // namespace nenenib::core
