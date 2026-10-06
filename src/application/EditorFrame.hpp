#pragma once

#include "Appearance.hpp"
#include "CaretView.hpp"
#include "CommandLine.hpp"
#include "CommandPaletteView.hpp"
#include "CompositionView.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ImeStance.hpp"
#include "InputLineView.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "Palette.hpp"
#include "SettingsIssue.hpp"
#include "StatusItems.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarTarget.hpp"
#include "VimMode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::application
{
// UI が写すだけの表示値（ARC-011）。どのメンバーも検証済みの値なので公開 aggregate。
// lines は見えている行だけで、本文の全体は載らない（ADR 0009 の決定 6）。
struct EditorFrame
{
    std::vector<LineView> lines;
    CaretView caret;
    core::LineNumber first_visible;
    std::size_t total_lines;
    core::Appearance appearance;
    core::Palette palette;
    core::EditMode mode;
    // Vim のモード。窓は NORMAL のあいだ IME を切るのにこれを読む（ADR 0014 の決定 5）。
    // 通常モードのときは意味を持たない（mode_label にも出ない）。
    core::VimMode vim_mode;
    // IME の構え（ADR 0061 の決定 1）。ui はこの値の変わり目を実行するだけ。
    ImeStance ime;
    std::string_view mode_label;
    // 録画中のマクロの名前（`q{a-z}` から `q` まで・ADR 0046 の決定 8）。Vim モードのあいだ
    // だけ値を持ち、renderer がモード表示の隣に `recording @a` を muted で描く。
    std::optional<char> recording;
    // 本文の変換中の文字列。キャレットの位置に差し込んで描く（ADR 0014 の決定 2）。
    std::optional<CompositionView> composition;
    // 面の入力行の変換中の文字列（ADR 0061 の決定 4）。面が開いていればこちらに載り、composition
    // は空。両方が同時に値を持つことは無い。
    std::optional<CompositionView> command_composition;
    DocumentView document;
    std::array<core::DisplayText, core::status_item_count> status_items;
    core::EditorSettings settings;
    // 起動時と設定変更時だけ告知する。本文の次のキーで繰り返し表示しない（ADR 0020）。
    std::optional<SettingsIssue> settings_failure;
    // 開いている入力行の見え方（Ex・設定一覧・検索の 1 本・ADR 0032 の決定 1）。
    std::optional<core::InputLineView> command_line;
    std::optional<core::DisplayText> command_message;
    std::optional<CommandPaletteView> command_palette;
    // 帯の順のタブの表示値（ADR 0056 の決定 7）。アクティブの分は document と同じ値で、ほかは
    // 置いたときに作った値を並べるだけ。active_tab は帯の上のアクティブの位置（0 始まり）。
    std::vector<DocumentView> tabs;
    std::size_t active_tab;
    // 帯の送り量（DIP・アクティブなタブが見える所へ application が直した値）と、マウスを載せて
    // いる帯の要素（ADR 0056 の決定 7）。renderer は配置の入力にこの 2 つを渡す。
    std::int32_t tab_scroll;
    std::optional<core::TitleBarTarget> hovered;
    // 最後の 1 つのタブを閉じる意図の 1 回だけ立つ（D22）。ui はこれを見て窓を閉じる。
    bool closing;
    // Ex の `:tabclose` が閉じたいタブの帯の位置（ADR 0057 の決定 6）。closing と同じく 1 意図の
    // 1 回だけ立つ。ui は意図を送った結果を受ける 1 か所でこれを見て、× と同じ閉じる流れ
    // （未保存なら確認・最後の 1 本なら窓を閉じる）を呼ぶ。controller はタブを閉じない。
    std::optional<std::size_t> close_request;
    // 操作の一覧で選ばれた操作（ADR 0078 の決定 8）。close_request と同じく 1 意図の 1 回だけ立つ。
    // ui は意図を送った結果を受ける 1 か所でこれを見て run_operation を呼ぶ。controller は操作を
    // 実行しない。
    std::optional<core::EditorOperation> operation_request;
};

// 変換中か（本文と面の入力行のどちらかに変換がある・ADR 0061 の決定 4）。ui が変換中かを見るのは
// この 1 本。
[[nodiscard]] inline bool composing(const EditorFrame &frame) noexcept
{
    return frame.composition.has_value() || frame.command_composition.has_value();
}

// 表示値から作る帯の配置の入力（ADR 0056 の決定 8・9）。renderer の描画と窓の hit test・
// クリック・hover が同じ値を渡すための 1 本。width は帯の幅（物理画素）。
[[nodiscard]] inline core::TitleBarInput title_bar_input(const EditorFrame &frame,
                                                         std::int32_t width, std::uint32_t dpi)
{
    return core::TitleBarInput{
        width, dpi, frame.tabs.size(), frame.active_tab, frame.tab_scroll, frame.hovered};
}
} // namespace nenenib::application
