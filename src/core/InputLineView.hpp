#pragma once

#include "InputLinePrompt.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// UI が写すだけの入力行の見え方（ARC-011 / ADR 0032 の決定 1）。Ex・設定一覧・検索の
// どれもこの 1 つの値に畳んでから描くので、入力行の描画・clip・caret 追従は 1 本である。
// どのメンバーも検証済みの値なので公開 aggregate（メソッドは持たない）。
struct InputLineView
{
    InputLinePrompt prompt;
    // 入力された 1 行（プロンプトの文字は含まない）。
    std::string text;
    // text の中のキャレットの位置（UTF-8 の境界）。
    Offset caret;
    // Tab の補完候補。補完を持たない入力行では空。
    std::vector<std::string> completions;
    // 選ばれている候補。補完していないときは空。
    std::optional<std::size_t> completion_index;
};
} // namespace nenenib::core
