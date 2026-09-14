#pragma once

#include "Offset.hpp"
#include "TextFailure.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::core
{
// UTF-8 の検証と走査の唯一の場所（ARC-001 / CPP-014）。DisplayText の検証も TextBuffer の
// 桁の数えもここを通る。<locale> / <codecvt> は使わない（ADR 0007）。

// 正しい UTF-8 なら code point の数を返す。過長符号化・サロゲート・U+10FFFF 超は拒否する。
[[nodiscard]] std::expected<std::size_t, TextFailure> validate_utf8(std::string_view text) noexcept;

// 検証済みの本文の code point の数。継続バイト以外を数えるだけなので分岐を持たない。
[[nodiscard]] std::size_t code_point_count(std::string_view text) noexcept;

// 制御文字（U+0000〜U+001F と U+007F）を含むか。これらは UTF-8 では必ず 1 バイトなので、
// 検証済みの本文では生バイトを見るだけで足りる。
[[nodiscard]] bool has_control_character(std::string_view text) noexcept;

// at が code point の先頭（または末尾の直後）か。
[[nodiscard]] bool is_boundary(std::string_view text, Offset at) noexcept;

// at の次／前の code point の先頭。末尾・先頭では動かない。
[[nodiscard]] Offset next_code_point(std::string_view text, Offset at) noexcept;
[[nodiscard]] Offset previous_code_point(std::string_view text, Offset at) noexcept;
} // namespace nenenib::core
