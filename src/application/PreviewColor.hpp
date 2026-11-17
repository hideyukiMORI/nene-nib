#pragma once

#include "SyntaxRole.hpp"

#include <cstddef>

namespace nenenib::application
{
// プレビュー欄の窓の 1 行の構文の色の区間（ADR 0189 の決定 5・段 P2）。PreviewLine::text（描画の
// 形）の中のバイトの半開区間と役割。行の中では昇順で重ならない。値だけを運ぶ公開 aggregate
// （CPP-003）。
struct PreviewColor
{
    std::size_t begin;
    std::size_t end;
    core::SyntaxRole role;
};
} // namespace nenenib::application
