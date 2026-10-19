#pragma once

#include "Appearance.hpp"
#include "CaretView.hpp"
#include "CommandLine.hpp"
#include "CommandPaletteView.hpp"
#include "CompositionView.hpp"
#include "DisplayForm.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "GridRowView.hpp"
#include "ImeStance.hpp"
#include "InputLineView.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "OperationGuide.hpp"
#include "Palette.hpp"
#include "PaneFrame.hpp"
#include "PaneSide.hpp"
#include "SettingsIssue.hpp"
#include "SplitLayout.hpp"
#include "StatusItems.hpp"
#include "TextLayoutFailure.hpp"
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
    // 入力行（面・Ex の行・検索の行）の変換中の文字列（ADR 0061 の決定 4）。入力行が開いていれば
    // こちらに載り、composition は空。両方が同時に値を持つことは無い。
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
    // 開いた文書に格子表示を聞く（ADR 0140 の決定 2）。EditorDelivery と同じ値。
    bool display_question;
    core::OperationGuide guide;
    std::optional<TextLayoutFailure> layout_failure{};
    float horizontal = 0;
    // 現在の文書の表示形式（ADR 0140 の決定 3）。本文の文字組みの保持は表示形式ごとに意味が違うので
    // frame の鍵に入れる。renderer は読むだけ（ステータスバーの印）。
    core::DisplayForm display = core::DisplayForm::text;
    // 開いたときの問いの答えを待っている（ADR 0140 の決定 2）。lines は空で、renderer は本文を
    // 描かず背景と帯とステータスバーだけを描く（本文の文字組みを作らない）。
    bool display_pending = false;
    // 格子の見えている画面行（ADR 0140 の決定 5）。display が grid のときだけ値を持ち、lines は空。
    // 画面の下に半分かかる画面行も含む。
    std::vector<GridRowView> grid{};
    // grid を作るのに 1 文字ずつ歩いた塊の部分の数（GridCells::scanned・数える口）。ASCII の行は
    // 割り算だけなので 0。
    std::size_t grid_scanned = 0;
    // grid の検索の当たりを照合するのに本文から読んだバイト数（数える口・#556）。
    // 見えている画面行の範囲に、一致の長さの上限ぶんの余裕と文脈の 1 文字を
    // 前後に足した分だけで、行の全体ではない。
    std::size_t grid_searched = 0;
    // 枠の配置・操作中の枠の側ともう 1 つの枠（ADR 0148 の決定 8）。1 枠では none・first・値なし。
    // 描く所は pane_layouts の first / second のうち active_side の側に操作中の枠を置く。
    core::SplitLayout layout = core::SplitLayout::none;
    core::PaneSide active_side = core::PaneSide::first;
    std::optional<PaneFrame> other{};
};

// 変換中か（本文と入力行のどちらかに変換がある・ADR 0061 の決定 4）。ui が変換中かを見るのは
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
