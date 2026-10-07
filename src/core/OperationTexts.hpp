#pragma once

#include "EditorOperation.hpp"
#include "OperationText.hpp"

#include <array>

namespace nenenib::core
{
// 操作の名前・読み・短い説明の表（ADR 0078 の決定 5）。EditorOperation の順に全値を 1 回ずつ持つ
// （OperationTexts.cpp の static_assert が守る）。鍵の表示名はここに書かず key_chord_label で作る。
inline constexpr std::array<OperationText, 16> operation_texts{{
    {EditorOperation::open_file, "ファイルを開く", "ふぁいるをひらく",
     "保存してあるファイルを選んで開きます"},
    {EditorOperation::save, "保存", "ほぞん", "いまのファイルに上書きします"},
    {EditorOperation::save_as, "名前を付けて保存", "なまえをつけてほぞん",
     "別の名前のファイルに保存します"},
    {EditorOperation::new_tab, "新しいタブ", "あたらしいたぶ", "空の文書を新しいタブで開きます"},
    {EditorOperation::close_tab, "タブを閉じる", "たぶをとじる", "いまのタブを閉じます"},
    {EditorOperation::recent_tab, "前に使ったタブへ", "まえにつかったたぶへ",
     "最近使った順にタブを移ります"},
    {EditorOperation::recent_tab_back, "前に使ったタブへ（逆向き）",
     "まえにつかったたぶへぎゃくむき", "最近使った順を逆にたどります"},
    {EditorOperation::list_files, "ファイルとタブの一覧", "ふぁいるとたぶのいちらん",
     "タブ・ブックマーク・履歴・同じフォルダから探します", "一覧"},
    {EditorOperation::list_operations, "操作の一覧", "そうさのいちらん", "この一覧を開きます",
     "ヘルプ"},
    {EditorOperation::toggle_bookmark, "ブックマークに付ける・外す", "ぶっくまーくにつけるはずす",
     "いまのファイルをブックマークに付け外しします"},
    {EditorOperation::undo, "元に戻す", "もとにもどす", "直前の編集を取り消します"},
    {EditorOperation::redo, "やり直す", "やりなおす", "取り消した編集をもう一度行います"},
    {EditorOperation::font_larger, "文字を大きく", "もじをおおきく",
     "本文の文字を 1 段大きくします"},
    {EditorOperation::font_smaller, "文字を小さく", "もじをちいさく",
     "本文の文字を 1 段小さくします"},
    {EditorOperation::font_reset, "文字の大きさを戻す", "もじのおおきさをもどす",
     "本文の文字を既定の大きさに戻します"},
    {EditorOperation::toggle_mode, "通常 / Vim を切り替える", "つうじょうびむをきりかえる",
     "編集のしかたを通常と Vim で切り替えます"},
}};

// 操作の文字の行。switch ではなく表を引く。
[[nodiscard]] const OperationText &operation_text(EditorOperation operation) noexcept;
} // namespace nenenib::core
