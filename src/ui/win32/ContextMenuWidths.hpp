#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// メニューの項目の名前と鍵の表示の最も広い幅（物理画素・切り上げ・ADR 0078 の追補 #1051）。
// core::ContextMenuRequest の name_width と key_width に渡す。
struct ContextMenuWidths
{
    std::int32_t name;
    std::int32_t key;
};
} // namespace nenenib::ui::win32
