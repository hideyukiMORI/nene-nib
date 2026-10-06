#pragma once

#include <cstdint>

namespace nenenib::core
{
// 鍵と F1 の一覧が実行する操作の閉じた一覧（ADR 0078 の決定 1）。操作を足すのはここに 1 値で、
// 割り当ての表 operation_bindings と文字の表 operation_texts に行を足し、落ちた switch を直す
// （CPP-002）。文字の表はこの順に並べる。
enum class EditorOperation : std::uint8_t
{
    open_file,
    save,
    save_as,
    new_tab,
    close_tab,
    recent_tab,
    recent_tab_back,
    list_files,
    list_operations,
    toggle_bookmark,
    undo,
    redo,
    font_larger,
    font_smaller,
    font_reset,
    toggle_mode
};
} // namespace nenenib::core
