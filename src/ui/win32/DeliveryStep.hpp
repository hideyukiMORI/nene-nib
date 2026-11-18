#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 意図を送った後に窓が後を追う 1 段（#1854・ClosingStep の形・#1340）。
// 順は DeliverySteps.hpp の delivery_order、要るかは delivery_step_due が決め、
// run_delivery_steps が流し、EditorWindow::delivery_step が 1 段ずつ写す。
// 値が増えたら写す側の `switch` が落ちる（CPP-002）。
enum class DeliveryStep : std::uint8_t
{
    // 入力行が閉じて変換が消えたら、IME の側の変換も取り消す（ADR 0061 の決定 3）。
    cancel_command_composition,
    // 設定の面が閉じたら、押しの覚えを消し、探す欄の変換を IME の側に残さない（#1836 のレビュー
    // M1）。 IME を開け閉めする follow_ime より前（閉じるときに IME が確定を本文へ送らない）。
    cancel_settings_composition,
    // 鍵の対応の待ちのタイマー（ADR 0012）。
    follow_key_map_timer,
    // タブの札のタイマー。
    follow_tab_tip_timer,
    // 字の大きさを変えた意図の後の知らせ。
    announce_font_size,
    // 外観と字体（字体が変われば body_view を送り直す）。
    follow_look,
    follow_split,
    follow_explorer,
    follow_minimap,
    // IME を開け閉めする（ADR 0061）。
    follow_ime
};
} // namespace nenenib::ui::win32
