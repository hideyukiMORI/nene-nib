#pragma once

#include "InvisibleSpan.hpp"
#include "LineBreakMark.hpp"

#include <optional>
#include <vector>

namespace nenenib::application
{
// 見えている 1 行の不可視文字（ADR 0193 の決定 1・10・#1977）。spans は本文の桁の区間（昇順・
// 重ならない）、line_break は行の終わりの改行の形（改行の無い最後の行・Vim のモードで隠す最後の
// 改行・`list_eol` がオフでは値なし）。renderer は spans の 1 字ごとと行の終わりに印を描く。公開
// aggregate（CPP-003）。
struct LineInvisibles
{
    std::vector<core::InvisibleSpan> spans;
    std::optional<core::LineBreakMark> line_break = std::nullopt;
};
} // namespace nenenib::application
