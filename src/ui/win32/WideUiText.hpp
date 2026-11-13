#pragma once

#include "DisplayLanguage.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "Utf16.hpp"

#include <initializer_list>
#include <string>
#include <string_view>

namespace nenenib::ui::win32
{
// ui が自分で OS の部品に渡す文（MessageBox・ShutdownBlockReason・ファイルの選択の絞り込み・既定の
// ファイル名）を表から引いて UTF-16 にする（ADR 0123 の追補 2026-11-13 の決定 11・#1019）。
// 表の文は正しい UTF-8（UiTexts.cpp の static_assert）なので変換は失敗しない。
// 呼ぶのは箱を出すときだけで、打鍵と frame の道には置かない。
[[nodiscard]] inline std::wstring wide_ui_text(core::UiMessage message,
                                               core::DisplayLanguage language)
{
    return core::to_utf16(core::ui_text(message, language)).value();
}

// 置き場 {0} {1} を埋めた文（決定 5）。埋める値は呼び出し元が定数から作った ASCII の数と単位。
[[nodiscard]] inline std::wstring
wide_ui_text_filled(core::UiMessage message, core::DisplayLanguage language,
                    std::initializer_list<std::string_view> values)
{
    return core::to_utf16(core::ui_text_filled(message, language, values)).value();
}
} // namespace nenenib::ui::win32
