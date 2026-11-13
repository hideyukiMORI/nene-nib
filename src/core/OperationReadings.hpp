#pragma once

#include "EditorOperation.hpp"
#include "OperationReading.hpp"

#include <array>
#include <string_view>

namespace nenenib::core
{
// 操作の読み（ひらがな）の表（ADR 0078 の決定 5・ADR 0123 の追補 2026-11-13 の決定 6）。画面には
// 出さず、表示言語が日本語のときだけ F1 の一覧の絞り込みで名前と並べて照合する（英語と中国語は
// 名前・鍵の表示名で探す・拼音の読みは持たない）。EditorOperation の順に全値を 1 回ずつ持つ
// （OperationTexts.cpp の static_assert が守る）。
inline constexpr std::array<OperationReading, 38> operation_readings{{
    {EditorOperation::open_file, "ふぁいるをひらく"},
    {EditorOperation::save, "ほぞん"},
    {EditorOperation::save_as, "なまえをつけてほぞん"},
    {EditorOperation::new_tab, "あたらしいたぶ"},
    {EditorOperation::close_tab, "たぶをとじる"},
    {EditorOperation::recent_tab, "まえにつかったたぶへ"},
    {EditorOperation::recent_tab_back, "まえにつかったたぶへぎゃくむき"},
    {EditorOperation::list_files, "ふぁいるとたぶのいちらん"},
    {EditorOperation::list_operations, "そうさのいちらん"},
    {EditorOperation::toggle_bookmark, "ぶっくまーくにつけるはずす"},
    {EditorOperation::undo, "もとにもどす"},
    {EditorOperation::redo, "やりなおす"},
    {EditorOperation::font_larger, "もじをおおきく"},
    {EditorOperation::font_smaller, "もじをちいさく"},
    {EditorOperation::font_reset, "もじのおおきさをもどす"},
    {EditorOperation::toggle_mode, "つうじょうびむをきりかえる"},
    {EditorOperation::split_vertical, "さゆうにわける"},
    {EditorOperation::split_horizontal, "じょうげにわける"},
    {EditorOperation::next_pane, "となりのわくへ"},
    {EditorOperation::close_pane, "わくをとじる"},
    {EditorOperation::toggle_preview, "まーくだうんぷれびゅー"},
    {EditorOperation::paste_plain, "もじだけをはりつけ"},
    {EditorOperation::paste_html, "えいちてぃーえむえるのままはりつけ"},
    {EditorOperation::toggle_explorer, "えくすぷろーらーをだすけす"},
    {EditorOperation::choose_explorer_root, "るーとのふぉるだをえらぶ"},
    {EditorOperation::insert_date_time, "にちじをそうにゅう"},
    {EditorOperation::choose_encoding, "もじこーどをえらぶ"},
    {EditorOperation::choose_line_ending, "かいぎょうをえらぶ"},
    {EditorOperation::find, "けんさく"},
    {EditorOperation::find_next, "つぎをけんさく"},
    {EditorOperation::find_previous, "まえをけんさく"},
    {EditorOperation::replace, "ちかん"},
    {EditorOperation::go_to_line, "ぎょうへいどう"},
    {EditorOperation::preview_full, "まーくだうんぷれびゅーぜんめん"},
    {EditorOperation::print, "いんさつ"},
    {EditorOperation::register_shell, "うぃんどうずのみぎくりっくにとうろくする"},
    {EditorOperation::unregister_shell, "とうろくをはずす"},
    {EditorOperation::choose_language, "ひょうじげんごをえらぶ"},
}};

// 操作の読み。switch ではなく表を引く。
[[nodiscard]] std::string_view operation_reading(EditorOperation operation) noexcept;
} // namespace nenenib::core
