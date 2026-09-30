#pragma once

#include "CommandInput.hpp"
#include "EditMode.hpp"
#include "VimMode.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::application
{
// IME をどうするかの構え（ADR 0061 の決定 1）。決めるのは ime_stance_of の 1 本で、ui は frame に
// 載った値の変わり目を実行するだけ。値が増えたら写し先の足りない `switch` が落ちる（CPP-002）。
enum class ImeStance : std::uint8_t
{
    // 使う人が残した状態に戻す（通常モードと Vim の INSERT で、入力行が無いとき）。
    as_left,
    // 閉じたままにする（Vim の NORMAL / VISUAL と、Ex の行・検索の行が開いているとき）。
    closed,
    // 入るときに 1 度閉じて、あとは使う人に任せる（Ctrl+P の面が開いているとき・D32）。
    closed_once
};

// 編集のモード・Vim のモード・開いている入力から構えを決める純関数（ADR 0061 の決定 1）。
// 入力があれば入力の種類が決め、無ければモードが決める。
[[nodiscard]] ImeStance ime_stance_of(core::EditMode mode, core::VimMode vim_mode,
                                      const std::optional<CommandInput> &input);
} // namespace nenenib::application
