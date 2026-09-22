#pragma once

#include "CommandEdit.hpp"
#include "ExFailure.hpp"
#include "Offset.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 1 行の入力とその中のキャレット（ADR 0032 の決定 1）。Ex の CommandLine と検索の SearchLine は
// 編集の規則をこの 2 つの純関数だけで共有する（ARC-001）。位置は UTF-8 の境界だけを通る。
// 不変条件を守るのは下の 2 つの関数で、外へ見せる型（CommandLine / SearchLine）が値を閉じ込める。
struct InputText
{
    std::string text;
    Offset caret;
};

// キャレットの位置に 1 行を差し込む。長すぎる・制御文字・不正な UTF-8 は閉じた失敗（CPP-005）。
[[nodiscard]] std::expected<InputText, ExFailure> inserted_input(const InputText &input,
                                                                 std::string_view text);
// 左右・Home / End・Backspace / Delete。補完の 2 つはここでは動かない（持つ型が決める）。
[[nodiscard]] InputText edited_input(const InputText &input, CommandEdit edit);
} // namespace nenenib::core
