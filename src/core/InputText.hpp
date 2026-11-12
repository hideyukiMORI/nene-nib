#pragma once

#include "CommandEdit.hpp"
#include "ExFailure.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 入力行（Ex の `:`・検索の `/` `?`・通常モードの「検索: 」「置換後: 」「行: 」・Ctrl+P の面）が
// 確定できる長さの上限（バイト・ADR 0022 の追補 #1568）。Vim に実質の上限は無いので、実用の
// 長さ（`:s` の長い pattern・260 字を超えるパス）が入る値にする。値はここの 1 つだけで、Ex の
// 評価（evaluate_ex）も同じ値で断る。
inline constexpr std::size_t input_line_limit = 4096;
// 入力行が持てる最大（上限の 2 倍）。上限を超えた分も持って「あふれた」行にし、Backspace で
// 上限の内へ戻せるようにする。これを超える差し込みは捨てて印 lost を立てる（#1568）。
inline constexpr std::size_t input_line_hold = 2 * input_line_limit;
// 入力行を描くときに文字組みする窓（キャレットの前と後ろのバイト数・#1568）。長い行で打つたびに
// 全部を組み直さない。前は入力欄の幅より十分に長く（横の送りがキャレットを右端に保つ）、後ろは
// キャレットの右に見える分。
inline constexpr std::size_t input_line_window_before = 1024;
inline constexpr std::size_t input_line_window_after = 256;

// 1 行の入力とその中のキャレット（ADR 0032 の決定 1）。Ex の CommandLine と検索の SearchLine は
// 編集の規則をこの 2 つの純関数だけで共有する（ARC-001）。位置は UTF-8 の境界だけを通る。
// 不変条件を守るのは下の 2 つの関数で、外へ見せる型（CommandLine / SearchLine）が値を閉じ込める。
struct InputText
{
    std::string text;
    Offset caret;
    // 差し込みを input_line_hold で捨てた印（#1568）。行が打たれた（流された）ものと違うので、
    // 行を閉じるまで消えず、確定は断られる。
    bool lost = false;
};

// キャレットの位置に 1 行を差し込む。制御文字・不正な UTF-8 は閉じた失敗（CPP-005）。長さでは
// 失敗しない: 上限を超えた行は overflows が真になり、確定で丸ごと断られる（#1568）。持てる最大
// （input_line_hold）を超える差し込みは文字を変えずに lost を立てる。
[[nodiscard]] std::expected<InputText, ExFailure> inserted_input(const InputText &input,
                                                                 std::string_view text);
// 左右・Home / End・Backspace / Delete。補完と一覧の送りはここでは動かない（持つ型が決める）。
[[nodiscard]] InputText edited_input(const InputText &input, CommandEdit edit);
// 行があふれているか（上限を超えている・差し込みを捨てた）。あふれた行の確定は断る（#1568）。
[[nodiscard]] bool overflows(const InputText &input) noexcept;
// 入力行の描画が文字組みする範囲（#1568）。from..to（キャレットと変換中の文字列）を含み、前は
// input_line_window_before、後ろは input_line_window_after バイトまでを code point の境目で
// 取る。from と to は text の code point の境目で、from <= to <= text.size()。
[[nodiscard]] OffsetRange input_line_window(std::string_view text, Offset from, Offset to) noexcept;
} // namespace nenenib::core
