#pragma once

#include "Offset.hpp"

#include <string>

namespace nenenib::core
{
// 要素の text の中のリンクの半開区間 [begin, end) とその宛先（ADR 0151 の追補 2026-11-19・#1869）。
// 位置は link の装飾（PreviewSpan）と同じ要素の text のバイト位置。target は md4c の href を実体参照と
// `\` のエスケープを解いた綴り（`%` の符号は解かない）で、裸の URL の `www.` は `http://` を、メールは
// `mailto:` を前に付けた形。種類は link_destination が決める。比較は非メンバー（CPP-003）。
struct PreviewLink
{
    Offset begin;
    Offset end;
    std::string target;
};

[[nodiscard]] inline bool operator==(const PreviewLink &left, const PreviewLink &right)
{
    return left.begin == right.begin && left.end == right.end && left.target == right.target;
}
} // namespace nenenib::core
