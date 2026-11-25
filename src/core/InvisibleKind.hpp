#pragma once

#include <cstdint>

namespace nenenib::core
{
// 不可視文字の区間の種類（ADR 0193 の決定 1・10・#1977）。space は行末でない空白（`listspace=all`
// のときだけ）、trailing_space は行末の空白の列（#1011 の「見つけたい空白」・強めの色）、tab は
// Tab、ideographic_space は全角空白（U+3000・強めの色）。閉じた和で、renderer の印の形と色は
// switch で引く（CPP-002）。
enum class InvisibleKind : std::uint8_t
{
    space,
    trailing_space,
    tab,
    ideographic_space
};
} // namespace nenenib::core
