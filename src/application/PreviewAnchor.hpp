#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::application
{
// 窓の中心にする全文検索の当たり（ADR 0189 の決定 4）。行と、その行の行頭からのバイトの半開区間。
// 値だけを運ぶ公開 aggregate（CPP-003）。
struct PreviewAnchor
{
    core::LineNumber line;
    std::size_t match_begin;
    std::size_t match_end;
};
} // namespace nenenib::application
