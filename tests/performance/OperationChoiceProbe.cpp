#include "OperationChoiceProbe.hpp"
#include "CommandChoice.hpp"
#include "Milestone.hpp"
#include "OperationChoiceSample.hpp"
#include <array>
#include <vector>
namespace nenenib::tests::performance
{
namespace
{
constexpr std::array<OperationChoiceExpected, 2> expected_0{
    {{core::EditorOperation::save, 1U, "保存", 2U, "いまのファイルに上書きします", 14U, "Ctrl+S",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::save_as, 2U, "名前を付けて保存", 8U, "別の名前のファイルに保存します",
      15U, "Ctrl+Shift+S", "", core::CommandChoiceKind::operate, std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 0> expected_1{{

}};
constexpr std::array<OperationChoiceExpected, 26> expected_2{
    {{core::EditorOperation::open_file, 0U, "ファイルを開く", 7U,
      "保存してあるファイルを選んで開きます", 18U, "Ctrl+O", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::save, 1U, "保存", 2U, "いまのファイルに上書きします", 14U, "Ctrl+S",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::save_as, 2U, "名前を付けて保存", 8U, "別の名前のファイルに保存します",
      15U, "Ctrl+Shift+S", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::new_tab, 3U, "新しいタブ", 5U, "空の文書を新しいタブで開きます", 15U,
      "Ctrl+T", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_tab, 4U, "タブを閉じる", 6U, "いまのタブを閉じます", 10U,
      "Ctrl+W", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab, 5U, "前に使ったタブへ", 8U, "最近使った順にタブを移ります",
      14U, "Ctrl+Tab", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab_back, 6U, "前に使ったタブへ（逆向き）", 13U,
      "最近使った順を逆にたどります", 14U, "Ctrl+Shift+Tab", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::list_files, 7U, "ファイルとタブの一覧", 10U,
      "タブ・ブックマーク・履歴・同じフォルダから探します", 25U, "Ctrl+P", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::list_operations, 8U, "操作の一覧", 5U, "この一覧を開きます", 9U, "F1",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_bookmark, 9U, "ブックマークに付ける・外す", 13U,
      "いまのファイルをブックマークに付け外しします", 22U, "Ctrl+D", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::undo, 10U, "元に戻す", 4U, "直前の編集を取り消します", 12U, "Ctrl+Z",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::redo, 11U, "やり直す", 4U, "取り消した編集をもう一度行います", 16U,
      "Ctrl+Y", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_larger, 12U, "文字を大きく", 6U, "本文の文字を 1 段大きくします",
      16U, "Ctrl++", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_smaller, 13U, "文字を小さく", 6U, "本文の文字を 1 段小さくします",
      16U, "Ctrl+-", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_reset, 14U, "文字の大きさを戻す", 9U,
      "本文の文字を既定の大きさに戻します", 17U, "Ctrl+0", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::toggle_mode, 15U, "通常 / Vim を切り替える", 15U,
      "編集のしかたを通常と Vim で切り替えます", 22U, "Ctrl+Shift+Space", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::split_vertical, 16U, "左右に分ける", 6U,
      "本文を左右 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::split_horizontal, 17U, "上下に分ける", 6U,
      "本文を上下 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+Shift+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::next_pane, 18U, "隣の枠へ", 4U, "もう 1 つの枠へ移ります", 13U, "F6",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_pane, 19U, "枠を閉じる", 5U, "いまの枠を閉じて 1 つに戻します",
      17U, "Ctrl+Shift+F4", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_preview, 20U, "Markdown プレビュー", 14U,
      "Markdown のプレビューを右（下）の枠に出す・もう一度で閉じる", 34U, "Ctrl+Shift+M", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::paste_plain, 21U, "文字だけを貼り付け", 9U,
      "クリップボードの文字だけを貼ります（HTML を変換しない）", 30U, "Ctrl+Shift+V", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::paste_html, 22U, "HTML のまま貼り付け", 12U,
      "クリップボードの HTML をタグごと貼ります", 23U, "", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::toggle_explorer, 23U, "エクスプローラーを出す・消す", 14U,
      "ルートからのフォルダの木を左の面に出す・もう一度で消します", 29U, "Ctrl+Shift+E", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::choose_explorer_root, 24U, "ルートのフォルダを選ぶ", 11U,
      "エクスプローラーのルートにするフォルダを選びます", 24U, "", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::insert_date_time, 25U, "日時を挿入", 5U,
      "いまの時刻と日付をキャレットの位置に入れます（Vim は INSERT で）", 38U, "F5", "",
      core::CommandChoiceKind::operate, std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 24> expected_3{
    {{core::EditorOperation::open_file, 0U, "ファイルを開く", 7U,
      "保存してあるファイルを選んで開きます", 18U, "Ctrl+Shift+O", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::save, 1U, "保存", 2U, "いまのファイルに上書きします", 14U, "Ctrl+S",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::save_as, 2U, "名前を付けて保存", 8U, "別の名前のファイルに保存します",
      15U, "Ctrl+Shift+S", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::new_tab, 3U, "新しいタブ", 5U, "空の文書を新しいタブで開きます", 15U,
      "Ctrl+T", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_tab, 4U, "タブを閉じる", 6U, "いまのタブを閉じます", 10U,
      "Ctrl+F4", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab, 5U, "前に使ったタブへ", 8U, "最近使った順にタブを移ります",
      14U, "Ctrl+Tab", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab_back, 6U, "前に使ったタブへ（逆向き）", 13U,
      "最近使った順を逆にたどります", 14U, "Ctrl+Shift+Tab", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::list_files, 7U, "ファイルとタブの一覧", 10U,
      "タブ・ブックマーク・履歴・同じフォルダから探します", 25U, "Ctrl+P", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::list_operations, 8U, "操作の一覧", 5U, "この一覧を開きます", 9U, "F1",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_bookmark, 9U, "ブックマークに付ける・外す", 13U,
      "いまのファイルをブックマークに付け外しします", 22U, "Ctrl+Shift+D", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_larger, 12U, "文字を大きく", 6U, "本文の文字を 1 段大きくします",
      16U, "Ctrl++", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_smaller, 13U, "文字を小さく", 6U, "本文の文字を 1 段小さくします",
      16U, "Ctrl+-", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_reset, 14U, "文字の大きさを戻す", 9U,
      "本文の文字を既定の大きさに戻します", 17U, "Ctrl+0", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::toggle_mode, 15U, "通常 / Vim を切り替える", 15U,
      "編集のしかたを通常と Vim で切り替えます", 22U, "Ctrl+Shift+Space", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::split_vertical, 16U, "左右に分ける", 6U,
      "本文を左右 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::split_horizontal, 17U, "上下に分ける", 6U,
      "本文を上下 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+Shift+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::next_pane, 18U, "隣の枠へ", 4U, "もう 1 つの枠へ移ります", 13U, "F6",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_pane, 19U, "枠を閉じる", 5U, "いまの枠を閉じて 1 つに戻します",
      17U, "Ctrl+Shift+F4", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_preview, 20U, "Markdown プレビュー", 14U,
      "Markdown のプレビューを右（下）の枠に出す・もう一度で閉じる", 34U, "Ctrl+Shift+M", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::paste_plain, 21U, "文字だけを貼り付け", 9U,
      "クリップボードの文字だけを貼ります（HTML を変換しない）", 30U, "Ctrl+Shift+V", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::paste_html, 22U, "HTML のまま貼り付け", 12U,
      "クリップボードの HTML をタグごと貼ります", 23U, "", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::toggle_explorer, 23U, "エクスプローラーを出す・消す", 14U,
      "ルートからのフォルダの木を左の面に出す・もう一度で消します", 29U, "Ctrl+Shift+E", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::choose_explorer_root, 24U, "ルートのフォルダを選ぶ", 11U,
      "エクスプローラーのルートにするフォルダを選びます", 24U, "", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::insert_date_time, 25U, "日時を挿入", 5U,
      "いまの時刻と日付をキャレットの位置に入れます（Vim は INSERT で）", 38U, "F5", "",
      core::CommandChoiceKind::operate, std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 26> expected_4{
    {{core::EditorOperation::paste_html, 22U, "HTML のまま貼り付け", 12U,
      "クリップボードの HTML をタグごと貼ります", 23U, "", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::choose_explorer_root, 24U, "ルートのフォルダを選ぶ", 11U,
      "エクスプローラーのルートにするフォルダを選びます", 24U, "", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::list_operations, 8U, "操作の一覧", 5U, "この一覧を開きます", 9U, "F1",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::next_pane, 18U, "隣の枠へ", 4U, "もう 1 つの枠へ移ります", 13U, "F6",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::insert_date_time, 25U, "日時を挿入", 5U,
      "いまの時刻と日付をキャレットの位置に入れます（Vim は INSERT で）", 38U, "F5", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::open_file, 0U, "ファイルを開く", 7U,
      "保存してあるファイルを選んで開きます", 18U, "Ctrl+O", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::save, 1U, "保存", 2U, "いまのファイルに上書きします", 14U, "Ctrl+S",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::new_tab, 3U, "新しいタブ", 5U, "空の文書を新しいタブで開きます", 15U,
      "Ctrl+T", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_tab, 4U, "タブを閉じる", 6U, "いまのタブを閉じます", 10U,
      "Ctrl+W", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::list_files, 7U, "ファイルとタブの一覧", 10U,
      "タブ・ブックマーク・履歴・同じフォルダから探します", 25U, "Ctrl+P", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_bookmark, 9U, "ブックマークに付ける・外す", 13U,
      "いまのファイルをブックマークに付け外しします", 22U, "Ctrl+D", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::undo, 10U, "元に戻す", 4U, "直前の編集を取り消します", 12U, "Ctrl+Z",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::redo, 11U, "やり直す", 4U, "取り消した編集をもう一度行います", 16U,
      "Ctrl+Y", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_larger, 12U, "文字を大きく", 6U, "本文の文字を 1 段大きくします",
      16U, "Ctrl++", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_smaller, 13U, "文字を小さく", 6U, "本文の文字を 1 段小さくします",
      16U, "Ctrl+-", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::font_reset, 14U, "文字の大きさを戻す", 9U,
      "本文の文字を既定の大きさに戻します", 17U, "Ctrl+0", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::split_vertical, 16U, "左右に分ける", 6U,
      "本文を左右 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab, 5U, "前に使ったタブへ", 8U, "最近使った順にタブを移ります",
      14U, "Ctrl+Tab", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::save_as, 2U, "名前を付けて保存", 8U, "別の名前のファイルに保存します",
      15U, "Ctrl+Shift+S", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::split_horizontal, 17U, "上下に分ける", 6U,
      "本文を上下 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+Shift+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_preview, 20U, "Markdown プレビュー", 14U,
      "Markdown のプレビューを右（下）の枠に出す・もう一度で閉じる", 34U, "Ctrl+Shift+M", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::paste_plain, 21U, "文字だけを貼り付け", 9U,
      "クリップボードの文字だけを貼ります（HTML を変換しない）", 30U, "Ctrl+Shift+V", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_explorer, 23U, "エクスプローラーを出す・消す", 14U,
      "ルートからのフォルダの木を左の面に出す・もう一度で消します", 29U, "Ctrl+Shift+E", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_pane, 19U, "枠を閉じる", 5U, "いまの枠を閉じて 1 つに戻します",
      17U, "Ctrl+Shift+F4", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab_back, 6U, "前に使ったタブへ（逆向き）", 13U,
      "最近使った順を逆にたどります", 14U, "Ctrl+Shift+Tab", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::toggle_mode, 15U, "通常 / Vim を切り替える", 15U,
      "編集のしかたを通常と Vim で切り替えます", 22U, "Ctrl+Shift+Space", "",
      core::CommandChoiceKind::operate, std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 11> expected_5{
    {{core::EditorOperation::save, 1U, "保存", 2U, "いまのファイルに上書きします", 14U, "Ctrl+S",
      "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::open_file, 0U, "ファイルを開く", 7U,
      "保存してあるファイルを選んで開きます", 18U, "Ctrl+Shift+O", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::save_as, 2U, "名前を付けて保存", 8U, "別の名前のファイルに保存します",
      15U, "Ctrl+Shift+S", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_bookmark, 9U, "ブックマークに付ける・外す", 13U,
      "いまのファイルをブックマークに付け外しします", 22U, "Ctrl+Shift+D", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::split_horizontal, 17U, "上下に分ける", 6U,
      "本文を上下 2 つの枠に分けて同じ文書を出します", 24U, "Ctrl+Shift+\\", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_preview, 20U, "Markdown プレビュー", 14U,
      "Markdown のプレビューを右（下）の枠に出す・もう一度で閉じる", 34U, "Ctrl+Shift+M", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::paste_plain, 21U, "文字だけを貼り付け", 9U,
      "クリップボードの文字だけを貼ります（HTML を変換しない）", 30U, "Ctrl+Shift+V", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::toggle_explorer, 23U, "エクスプローラーを出す・消す", 14U,
      "ルートからのフォルダの木を左の面に出す・もう一度で消します", 29U, "Ctrl+Shift+E", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_pane, 19U, "枠を閉じる", 5U, "いまの枠を閉じて 1 つに戻します",
      17U, "Ctrl+Shift+F4", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab_back, 6U, "前に使ったタブへ（逆向き）", 13U,
      "最近使った順を逆にたどります", 14U, "Ctrl+Shift+Tab", "", core::CommandChoiceKind::operate,
      std::nullopt},
     {core::EditorOperation::toggle_mode, 15U, "通常 / Vim を切り替える", 15U,
      "編集のしかたを通常と Vim で切り替えます", 22U, "Ctrl+Shift+Space", "",
      core::CommandChoiceKind::operate, std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 5> expected_6{
    {{core::EditorOperation::close_tab, 4U, "タブを閉じる", 6U, "いまのタブを閉じます", 10U,
      "Ctrl+W", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::new_tab, 3U, "新しいタブ", 5U, "空の文書を新しいタブで開きます", 15U,
      "Ctrl+T", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab, 5U, "前に使ったタブへ", 8U, "最近使った順にタブを移ります",
      14U, "Ctrl+Tab", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::list_files, 7U, "ファイルとタブの一覧", 10U,
      "タブ・ブックマーク・履歴・同じフォルダから探します", 25U, "Ctrl+P", "",
      core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::recent_tab_back, 6U, "前に使ったタブへ（逆向き）", 13U,
      "最近使った順を逆にたどります", 14U, "Ctrl+Shift+Tab", "", core::CommandChoiceKind::operate,
      std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 2> expected_7{
    {{core::EditorOperation::close_tab, 4U, "タブを閉じる", 6U, "いまのタブを閉じます", 10U,
      "Ctrl+F4", "", core::CommandChoiceKind::operate, std::nullopt},
     {core::EditorOperation::close_pane, 19U, "枠を閉じる", 5U, "いまの枠を閉じて 1 つに戻します",
      17U, "Ctrl+Shift+F4", "", core::CommandChoiceKind::operate, std::nullopt}}};
constexpr std::array<OperationChoiceExpected, 0> expected_8{{

}};
constexpr std::array<OperationChoiceExpected, 0> expected_9{{

}};
constexpr std::array<OperationChoiceSample, 10> samples{
    {{"ほぞん", core::EditMode::ordinary, 1024U, 186368U, expected_0,
      "operation-choice-v1\nreading-"
      "ordinary\nordinary\nほぞん\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68"
      "820d98\n"},
     {"qqq", core::EditMode::ordinary, 1024U, 1024U, expected_1,
      "operation-choice-v1\nmiss-"
      "ordinary\nordinary\nqqq\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820"
      "d98\n"},
     {"", core::EditMode::ordinary, 256U, 841472U, expected_2,
      "operation-choice-v1\nall-"
      "ordinary\nordinary\n\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98"
      "\n"},
     {"", core::EditMode::vim, 256U, 798464U, expected_3,
      "operation-choice-v1\nall-"
      "vim\nvim\n\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n"},
     {" ", core::EditMode::ordinary, 256U, 841472U, expected_4,
      "operation-choice-v1\nspace-ordinary\nordinary\n "
      "\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n"},
     {"cTrL+s", core::EditMode::vim, 1024U, 1625088U, expected_5,
      "operation-choice-v1\nkey-vim\nvim\ncTrL+"
      "s\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n"},
     {"タブ", core::EditMode::ordinary, 1024U, 569344U, expected_6,
      "operation-choice-v1\nname-"
      "ordinary\nordinary\nタブ\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f6882"
      "0d98\n"},
     {"CTRL+F4", core::EditMode::vim, 1024U, 198656U, expected_7,
      "operation-choice-v1\nclose-key-vim\nvim\nCTRL+"
      "F4\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n"},
     {"上書き", core::EditMode::ordinary, 1024U, 1024U, expected_8,
      "operation-choice-v1\ndescription-"
      "miss\nordinary\n上書き\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d"
      "98\n"},
     {"ヘルプ", core::EditMode::ordinary, 1024U, 1024U, expected_9,
      "operation-choice-v1\nshort-name-"
      "miss\nordinary\nヘルプ\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d"
      "98\n"}}};

[[nodiscard]] bool display_equal(const core::CommandChoice &choice,
                                 const OperationChoiceExpected &expected)
{
    if (!choice.detail.has_value())
    {
        return false;
    }
    const auto &detail = choice.detail.value();
    return choice.label.text() == expected.label &&
           choice.label.code_point_count() == expected.label_code_points &&
           detail.text() == expected.detail &&
           detail.code_point_count() == expected.detail_code_points;
}
[[nodiscard]] bool choice_equal(const core::CommandChoice &choice,
                                const OperationChoiceExpected &expected)
{
    if (!choice.operation.has_value())
    {
        return false;
    }
    const auto operation = choice.operation.value();
    return operation == expected.operation &&
           static_cast<std::size_t>(operation) == expected.operation_ordinal &&
           choice.command == expected.command && choice.kind == expected.kind &&
           choice.origin == expected.origin && choice.key == expected.key &&
           display_equal(choice, expected);
}
[[nodiscard]] bool result_equal(std::span<const core::CommandChoice> result,
                                const OperationChoiceSample &sample)
{
    if (result.size() != sample.expected.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        if (!choice_equal(result[index], sample.expected[index]))
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] std::vector<std::string_view> held_views(std::span<const core::CommandChoice> choices)
{
    std::vector<std::string_view> views;
    for (const auto &choice : choices)
    {
        if (!choice.detail.has_value())
        {
            return {};
        }
        views.emplace_back(choice.label.text());
        views.emplace_back(choice.detail.value().text());
        views.emplace_back(choice.key);
    }
    return views;
}
[[nodiscard]] bool views_equal(std::span<const std::string_view> views,
                               const OperationChoiceSample &sample)
{
    if (views.size() != sample.expected.size() * 3U)
    {
        return false;
    }
    for (std::size_t index = 0; index < sample.expected.size(); ++index)
    {
        const auto &expected = sample.expected[index];
        const auto offset = index * 3U;
        if (views[offset] != expected.label || views[offset + 1U] != expected.detail ||
            views[offset + 2U] != expected.key)
        {
            return false;
        }
    }
    return true;
}
[[nodiscard]] std::uint64_t choice_checksum(const core::CommandChoice &choice)
{
    std::uint64_t result =
        choice.label.text().size() + choice.label.code_point_count() + choice.key.size();
    if (choice.detail.has_value())
    {
        const auto &detail = choice.detail.value();
        result += detail.text().size() + detail.code_point_count();
    }
    if (choice.operation.has_value())
    {
        result += static_cast<std::uint64_t>(choice.operation.value()) + 1U;
    }
    return result;
}
[[nodiscard]] std::uint64_t result_checksum(std::span<const core::CommandChoice> choices)
{
    std::uint64_t result = 1U + choices.size();
    for (const auto &choice : choices)
    {
        result += choice_checksum(choice);
    }
    return result;
}
[[nodiscard]] std::vector<core::CommandChoice> detached_choices(const OperationChoiceSample &sample)
{
    const std::string query(sample.query);
    return core::operation_choices(query, sample.mode);
}
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
pure_probe(const OperationChoiceSample &sample, application::TimingPort &timing)
{
    const std::string query(sample.query);
    const auto mode = sample.mode;
    const auto held = core::operation_choices(query, mode);
    const auto views = held_views(held);
    const auto detached = detached_choices(sample);
    const auto detached_views = held_views(detached);
    if (!result_equal(held, sample) || !views_equal(views, sample) ||
        !result_equal(detached, sample) || !views_equal(detached_views, sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < sample.rounds; ++index)
    {
        const auto choices = core::operation_choices(query, mode);
        checksum += result_checksum(choices);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || query != sample.query || mode != sample.mode ||
        !result_equal(core::operation_choices(query, mode), sample) ||
        !result_equal(held, sample) || !views_equal(views, sample) ||
        !result_equal(detached, sample) || !views_equal(detached_views, sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace
std::string operation_choice_input(OperationChoiceWorkload workload)
{
    return std::string(samples.at(static_cast<std::size_t>(workload)).metadata);
}
std::expected<std::uint64_t, ProbeFailure> probed_operation_choice(OperationChoiceWorkload workload,
                                                                   const std::string &input,
                                                                   application::TimingPort &timing)
{
    const auto &sample = samples.at(static_cast<std::size_t>(workload));
    if (input != sample.metadata)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return pure_probe(sample, timing);
}
} // namespace nenenib::tests::performance
