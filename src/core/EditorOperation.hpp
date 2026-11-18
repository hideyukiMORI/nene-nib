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
    toggle_mode,
    split_vertical,
    split_horizontal,
    next_pane,
    close_pane,
    toggle_preview,
    // 貼り付けの 2 つ（ADR 0157 の決定 2・3）。
    paste_plain,
    paste_html,
    // エクスプローラーの 2 つ（ADR 0162 の決定 7）。
    toggle_explorer,
    choose_explorer_root,
    // F5 の日時の挿入（ADR 0175 の決定 3）。
    insert_date_time,
    // 保存の形を選ぶ 2 つ（ADR 0010 の追補 #1049 の決定 2 の (c)）。
    choose_encoding,
    choose_line_ending,
    // 通常モードの検索・置換・行へ移動（ADR 0174 の決定 1）。鍵は通常モードの行だけで、Vim では
    // 鍵の無い行（F1 の一覧から選ぶと `/` と `:` の入力行・決定 2）。
    find,
    find_next,
    find_previous,
    replace,
    go_to_line,
    // 全面の Markdown プレビュー（ADR 0155 の改訂 16）。鍵なしで F1 の一覧から。
    preview_full,
    // 印刷（ADR 0177 の決定 1）。鍵は無く、F1 の一覧と Vim の `:hardcopy` から。
    print,
    // エクスプローラーへの登録の 2 つ（ADR 0178 の決定 3）。
    register_shell,
    unregister_shell,
    // 表示言語を選ぶ（ADR 0123 の追補 2026-11-13 の決定 8・#1019）。鍵なしで F1 の一覧から。
    choose_language,
    // バージョン情報（施主決定 D90・#1677・ADR 0078 の追補）。鍵なしで F1 の一覧と Vim の
    // `:version` から。ステータスバーの 1 行の知らせだけで、ページを開かない。
    show_version,
    // プロジェクトの全文検索（ADR 0167 の決定 1）。Ctrl+Shift+F で両モード・F1 の一覧と Vim の
    // `:grep` から。Ctrl+P の面を出どころ `!` で開く。
    search_project,
    // 切り取り・コピー・貼り付け・削除・すべて選択（ADR 0078 の追補 #1051）。鍵の行は通常モードの
    // Ctrl+X / C / V・Delete・Ctrl+A で、鍵を受ける道は ui の今の分岐のまま（Vim の INSERT と
    // エクスプローラーの焦点の振る舞いを変えない・TabShortcut と同じ形）。表の鍵は一覧と本文の
    // 右クリックのメニューの表示に使う。
    cut,
    copy,
    paste,
    delete_selection,
    select_all,
    // 探して見る面のプレビュー欄の置き方を次へ（ADR 0189 の決定 3）。面が開いている間の
    // Ctrl+Shift+P（ui の入力行の鍵）・見出しの右端の印・F1 の一覧から。面が閉じているときの
    // Ctrl+Shift+P は面を開く行（list_files）が先に当たるので、表の行は鍵なし。
    cycle_finder_preview,
    // 補完の面を開く（ADR 0190 の決定 5）。鍵は通常モードの Ctrl+Space だけ（VS Code・Visual Studio
    // と 同じ・D82）。Vim の INSERT の Ctrl+Space（Ctrl-@）は譲るので、Vim には行を置かない（INSERT
    // の Ctrl-n / Ctrl-p は engine の鍵・ADR 0169）。
    complete
};
} // namespace nenenib::core
