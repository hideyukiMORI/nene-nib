#pragma once

namespace nenenib::ui::win32
{
// 意図を送った後の段を決める値（#1854）。窓が deliver の入口で集め、
// delivery_step_due が読むだけ。
struct DeliveryFacts final
{
    // 送る前に入力行が変換を持っていたか・送った後も持っているか。
    bool command_composed = false;
    bool command_composing = false;
    // 意図が確定（CommitText）か取消（CancelComposition）か。IME が自分で終えたので触らない。
    bool composition_settled = false;
    // 送る前に設定の面が開いていたか・送った後も開いているか。
    bool settings_was_open = false;
    bool settings_open = false;
    // 意図が字の大きさの変更（AdjustFontSize）か。
    bool font_size_adjusted = false;
};
} // namespace nenenib::ui::win32
