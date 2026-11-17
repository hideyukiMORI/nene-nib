#pragma once

#include "ContextMenuSlot.hpp"
#include "EditorOperation.hpp"
#include "UiMessage.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 文字の表の 1 行（ADR 0078 の決定 5・ADR 0123 の追補 2026-11-13 の決定 6）。名前・説明・短い
// 名前は文の表の名前で、画面の言語の文は ui_text で引く。読みは operation_readings の別の表。
struct OperationText
{
    EditorOperation operation;
    UiMessage name;
    UiMessage description;
    // 無ければ通常名を使う。操作一覧の名前には使わない（ADR
    // 0079）。検索の対象にするのは英語と中国語 のときだけ（日本語は読みで探す・ADR 0123 の
    // 2026-11-13 の追補の決定 6）。
    std::optional<UiMessage> short_name{};
    // 本文の右クリックのメニューの置き場（ADR 0078 の追補 #1051）。無ければメニューに載らない。
    std::optional<ContextMenuSlot> context_menu{};
    // Vim のモードでメニューの行の右端に見せる Vim の鍵（`y` `ggVG` など）。無ければ空。訳さない。
    std::string_view vim_hint{};
};
} // namespace nenenib::core
