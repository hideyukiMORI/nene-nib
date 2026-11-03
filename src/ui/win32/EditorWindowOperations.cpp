#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "BeginFind.hpp"
#include "BeginGoToLine.hpp"
#include "BeginReplace.hpp"
#include "ClosePane.hpp"
#include "CloseTab.hpp"
#include "DocumentFormatKind.hpp"
#include "EditMode.hpp"
#include "ExplorerRootChosen.hpp"
#include "FileDialog.hpp"
#include "FileDrop.hpp"
#include "FileFailure.hpp"
#include "FontSizeAdjustment.hpp"
#include "InsertDateTime.hpp"
#include "Milestone.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenDocumentFormatList.hpp"
#include "OpenOperationList.hpp"
#include "OperationTexts.hpp"
#include "PrintDocument.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "SearchHop.hpp"
#include "SelectEditMode.hpp"
#include "SetPreviewPlacement.hpp"
#include "SettingsNotice.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchPane.hpp"
#include "SwitchTab.hpp"
#include "TabCommand.hpp"
#include "TextEncoding.hpp"
#include "ToggleBookmark.hpp"
#include "ToggleExplorer.hpp"
#include "UnsavedTab.hpp"
#include "VimSearchDirection.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
using detail::product_name;
using detail::widen;

namespace
{
constexpr wchar_t untitled_file[] = L"無題.txt";

// 格子表示の問いの文言（ADR 0140 の決定 2）。ここの 1 か所だけ。
constexpr wchar_t display_question[] = L"改行の無い巨大な行があります。格子表示で開きますか。";

// 失敗の理由は 1 行だけ出す。本文は変わらない（ADR 0010 の決定 9）。
[[nodiscard]] const wchar_t *reason_of(application::FileFailure failure) noexcept
{
    switch (failure)
    {
    case application::FileFailure::not_found:
        return L"ファイルが見つかりませんでした。";
    case application::FileFailure::access_denied:
        return L"ファイルを開く権限がありません。";
    case application::FileFailure::unreadable:
        return L"ファイルを読み取れませんでした。";
    case application::FileFailure::unwritable:
        return L"ファイルを保存できませんでした。元のファイルは変わっていません。";
    case application::FileFailure::already_exists:
        return L"ファイルは既に存在します。";
    case application::FileFailure::too_large:
        return L"1 GB を超えるファイルは開けません。";
    case application::FileFailure::too_large_to_decode:
        return L"64 MiB を超える Shift_JIS / UTF-16 は開けません。";
    case application::FileFailure::undecodable:
        return L"文字コードを判別できませんでした。";
    case application::FileFailure::unsupported_utf32:
        return L"UTF-32 のファイルは扱えません。UTF-8 か UTF-16 で保存し直してください。";
    case application::FileFailure::unencodable:
        return L"この文字コードでは保存できない文字があります。";
    case application::FileFailure::insufficient_memory:
        return L"メモリが足りず、ファイルを開けませんでした。";
    }
    std::unreachable();
}
} // namespace

void EditorWindow::close_tab(std::size_t tab)
{
    // 未保存なら映してから確かめる。取り消しか保存の失敗なら閉じない（ADR 0056 の決定 6）。
    if (application::tab_unsaved(controller_.documents(), tab))
    {
        send(application::SwitchTab{tab});
        if (!confirm_discard())
        {
            return;
        }
    }
    send(application::CloseTab{tab});
}

void EditorWindow::finish_display_question(bool question)
{
    if (question)
    {
        ask_display();
    }
}

// 開いた文書に格子表示を聞く（ADR 0140 の決定 2）。答えは必ず意図で返す: 「はい」は grid、
// 「いいえ」と問いを出せなかったとき（0）は text。答えが届くまで controller は本文の文字組みを
// 止めている。その中の send は印が立たないので再入は 1 段で止まる。
// 前後の節目は答えを待つ時間を計測器が開くの値から引くため（#689）。
void EditorWindow::ask_display()
{
    timing_.mark(core::Milestone::question_shown);
    const int answer =
        MessageBoxW(window_, display_question, product_name, MB_YESNO | MB_ICONQUESTION);
    timing_.mark(core::Milestone::question_answered);
    send(application::SelectDisplay{answer == IDYES ? core::DisplayForm::grid
                                                    : core::DisplayForm::text});
}

// 一覧から頼まれた操作は鍵と同じ run_operation へ。その中の send は operation_request が空なので
// 再入は 1 段で止まる（ADR 0078 の決定 8）。
void EditorWindow::finish_operation(std::optional<core::EditorOperation> operation_request)
{
    if (operation_request.has_value())
    {
        run_operation(operation_request.value());
    }
}

// frame を写し終えてから閉じる。窓を壊した後は古い frame で題名を上書きしない（ADR 0066）。
void EditorWindow::finish_tab_action(std::optional<std::size_t> close_request, bool closing)
{
    // :tabclose は × と同じ確認へ。その中の send は close_request が空なので再入は 1 段で止まる。
    if (close_request.has_value())
    {
        close_tab(close_request.value());
        return;
    }
    // Ex と通常のタブ操作が通る終了の 1 か所。確認は済んでいる。
    if (closing)
    {
        leave(application::SessionEnd::last_tab_closed);
    }
}

void EditorWindow::announce(const application::EditorDelivery &delivery)
{
    if (!delivery.document.last_failure.has_value())
    {
        return;
    }
    const auto failure = delivery.document.last_failure.value();
    if (failure == application::FileFailure::unencodable && delivery.document.path.has_value())
    {
        offer_utf8(delivery.document.path.value());
        return;
    }
    MessageBoxW(window_, reason_of(failure), product_name, MB_OK | MB_ICONWARNING);
}

void EditorWindow::announce_settings(const application::EditorDelivery &delivery)
{
    if (delivery.settings_failure.has_value())
    {
        MessageBoxW(window_, settings_notice(delivery.settings_failure.value()).c_str(),
                    product_name, MB_OK | MB_ICONWARNING);
    }
}

void EditorWindow::offer_utf8(const core::FilePath &path)
{
    const int answer = MessageBoxW(
        window_, L"この文字コードでは保存できない文字があります。UTF-8 で保存しますか。",
        product_name, MB_YESNO | MB_ICONWARNING);
    if (answer != IDYES)
    {
        return;
    }
    send(application::SaveDocument{path, core::TextEncoding::utf8});
}

void EditorWindow::open_document()
{
    // 開くは今の文書を置き換えないので確かめない（ADR 0056 の決定 5）。
    const auto chosen = choose_file_to_open(window_);
    if (!chosen.has_value())
    {
        return;
    }
    send(application::OpenDocument{chosen.value()});
}

void EditorWindow::open_dropped_files(WPARAM word)
{
    // 通知の modal loop に入る前に OS の転送メモリを解放する（ADR 0116）。
    const auto paths = take_dropped_files(std::bit_cast<HDROP>(word));
    end_selection_drag();
    for (const auto &path : paths)
    {
        if (window_ == nullptr)
        {
            break;
        }
        if (!path)
        {
            MessageBoxW(window_, reason_of(path.error()), product_name, MB_OK | MB_ICONWARNING);
            continue;
        }
        send(application::OpenDocument{path.value()});
    }
}

void EditorWindow::save_document()
{
    const auto delivery = controller_.delivery();
    if (!delivery.document.path.has_value())
    {
        save_document_as();
        return;
    }
    send(application::SaveDocument{delivery.document.path.value(), delivery.document.encoding});
}

void EditorWindow::save_document_as()
{
    const auto delivery = controller_.delivery();
    const std::wstring suggested = delivery.document.path.has_value()
                                       ? widen(delivery.document.path.value().file_name())
                                       : std::wstring(untitled_file);
    const auto chosen = choose_file_to_save(window_, suggested);
    if (!chosen.has_value())
    {
        return;
    }
    send(application::SaveDocument{chosen.value(), delivery.document.encoding});
}

bool EditorWindow::confirm_discard()
{
    // 確認の間に描画を失ったら、終わりは abandon が持つ（窓はもう壊れているかもしれない）。
    if (rendering_failed_)
    {
        return false;
    }
    if (controller_.delivery().document.save_state == core::SaveState::saved)
    {
        return true;
    }
    const int answer = MessageBoxW(window_, L"変更が保存されていません。保存しますか。",
                                   product_name, MB_YESNOCANCEL | MB_ICONWARNING);
    if (answer == IDNO)
    {
        return true;
    }
    if (answer != IDYES)
    {
        return false;
    }
    // 保存に失敗したか取り消したときは、まだ未保存のままなので続けない。
    save_document();
    return controller_.delivery().document.save_state == core::SaveState::saved;
}

bool EditorWindow::confirm_lost_tab()
{
    // 取消は無い（窓は描けないので続けられない）。保存に失敗したか保存先を選ばなかったときは、
    // 理由（announce のダイアログ）を見せた後でもう一度聞く。
    while (controller_.delivery().document.save_state != core::SaveState::saved)
    {
        const int answer = MessageBoxW(window_, L"描画できなくなったので閉じます。保存しますか。",
                                       product_name, MB_YESNO | MB_ICONWARNING);
        if (answer != IDYES)
        {
            return true;
        }
        save_document();
    }
    return true;
}

bool EditorWindow::confirm_unsaved_tabs(bool (EditorWindow::*confirm)())
{
    std::size_t from = 0;
    for (auto next = application::next_unsaved_tab(controller_.documents(), from); next.has_value();
         next = application::next_unsaved_tab(controller_.documents(), from))
    {
        send(application::SwitchTab{next.value()});
        // 映す途中で窓が壊れたら（入れ子の終わりが済んだら）続けない。
        if (window_ == nullptr || !std::invoke(confirm, this))
        {
            return false;
        }
        from = next.value() + 1;
    }
    return window_ != nullptr;
}

namespace
{
// 操作の表の 1 行（ADR 0078 の追補 #1188）。行の型を足さず std::pair の別名で書く（CPP-011）。
using OperationRunner = std::pair<core::EditorOperation, void (EditorWindow::*)()>;

// 表の i 行目が i 番目の値を持ち、どの行も関数を指す（OperationTexts.cpp の texts_in_order と
// 同じ形）。表の大きさは operation_texts.size() なので、行の書き忘れは {open_file, nullptr} で
// 埋まってここが落とし、行が多すぎれば初期化子の数でコンパイルが落ちる。
template <std::size_t Size>
[[nodiscard]] consteval bool
runners_in_order(const std::array<OperationRunner, Size> &rows) noexcept
{
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        if (std::to_underlying(rows[index].first) != index || rows[index].second == nullptr)
        {
            return false;
        }
    }
    return true;
}
} // namespace

// 操作 → メンバ関数の表（ADR 0078 の追補 #1188）。行は EditorOperation の順。private の関数を指す
// ので、表は EditorWindow の static メンバ関数の中に置く。操作を足すときは行を 1 つと小さな
// メンバ関数を 1 つ足す。
EditorWindow::OperationFunction EditorWindow::runner_of(core::EditorOperation operation) noexcept
{
    static constexpr std::array<OperationRunner, core::operation_texts.size()> runners{{
        {core::EditorOperation::open_file, &EditorWindow::open_document},
        {core::EditorOperation::save, &EditorWindow::save_document},
        {core::EditorOperation::save_as, &EditorWindow::save_document_as},
        {core::EditorOperation::new_tab, &EditorWindow::run_new_tab},
        {core::EditorOperation::close_tab, &EditorWindow::run_close_tab},
        {core::EditorOperation::recent_tab, &EditorWindow::run_recent_tab},
        {core::EditorOperation::recent_tab_back, &EditorWindow::run_recent_tab_back},
        {core::EditorOperation::list_files, &EditorWindow::run_list_files},
        {core::EditorOperation::list_operations, &EditorWindow::run_list_operations},
        {core::EditorOperation::toggle_bookmark, &EditorWindow::run_toggle_bookmark},
        {core::EditorOperation::undo, &EditorWindow::run_undo},
        {core::EditorOperation::redo, &EditorWindow::run_redo},
        {core::EditorOperation::font_larger, &EditorWindow::run_font_larger},
        {core::EditorOperation::font_smaller, &EditorWindow::run_font_smaller},
        {core::EditorOperation::font_reset, &EditorWindow::run_font_reset},
        {core::EditorOperation::toggle_mode, &EditorWindow::toggle_mode},
        {core::EditorOperation::split_vertical, &EditorWindow::run_split_vertical},
        {core::EditorOperation::split_horizontal, &EditorWindow::run_split_horizontal},
        {core::EditorOperation::next_pane, &EditorWindow::run_next_pane},
        {core::EditorOperation::close_pane, &EditorWindow::run_close_pane},
        {core::EditorOperation::toggle_preview, &EditorWindow::run_toggle_preview},
        {core::EditorOperation::paste_plain, &EditorWindow::run_paste_plain},
        {core::EditorOperation::paste_html, &EditorWindow::run_paste_html},
        {core::EditorOperation::toggle_explorer, &EditorWindow::run_toggle_explorer},
        {core::EditorOperation::choose_explorer_root, &EditorWindow::choose_explorer_root},
        {core::EditorOperation::insert_date_time, &EditorWindow::run_insert_date_time},
        {core::EditorOperation::choose_encoding, &EditorWindow::run_choose_encoding},
        {core::EditorOperation::choose_line_ending, &EditorWindow::run_choose_line_ending},
        {core::EditorOperation::find, &EditorWindow::run_find},
        {core::EditorOperation::find_next, &EditorWindow::run_find_next},
        {core::EditorOperation::find_previous, &EditorWindow::run_find_previous},
        {core::EditorOperation::replace, &EditorWindow::run_replace},
        {core::EditorOperation::go_to_line, &EditorWindow::run_go_to_line},
        {core::EditorOperation::preview_full, &EditorWindow::run_preview_full},
        {core::EditorOperation::print, &EditorWindow::run_print},
    }};
    static_assert(runners_in_order(runners),
                  "operation runners follow the order of EditorOperation and point to a function");
    return runners[std::to_underlying(operation)].second;
}

// 鍵と一覧が実行する唯一の口（ADR 0078 の決定 7）。中身は今ある ui の関数と意図をそのまま使い、
// controller に操作ごとの分岐を書かない。操作ごとの関数は runner_of の表が引く（追補 #1188）。
void EditorWindow::run_operation(core::EditorOperation operation)
{
    (this->*runner_of(operation))();
}

// タブの操作（ADR 0056 / 0058 / 0078）。
void EditorWindow::run_new_tab()
{
    run_tab_command(core::TabCommand::open);
}

void EditorWindow::run_close_tab()
{
    run_tab_command(core::TabCommand::close);
}

void EditorWindow::run_recent_tab()
{
    walk_recent_tab(core::TabCommand::next);
}

void EditorWindow::run_recent_tab_back()
{
    walk_recent_tab(core::TabCommand::previous);
}

// 意図を 1 つ送るだけの操作（一覧・ブックマークと、検索・置換・行へ移動）。検索の 5 つは
// どちらのモードで何をするかを controller が決める（Vim は `/` `:` `n` `N` の前置き・
// ADR 0174 の決定 1・2）。置換の 2 段は controller の BeginReplace が開く。
void EditorWindow::run_list_files()
{
    send(application::OpenCommandPalette{});
}

void EditorWindow::run_list_operations()
{
    send(application::OpenOperationList{});
}

void EditorWindow::run_toggle_bookmark()
{
    send(application::ToggleBookmark{});
}

void EditorWindow::run_find()
{
    send(application::BeginFind{});
}

void EditorWindow::run_find_next()
{
    send(application::SearchHop{core::VimSearchDirection::forward});
}

void EditorWindow::run_find_previous()
{
    send(application::SearchHop{core::VimSearchDirection::backward});
}

void EditorWindow::run_replace()
{
    send(application::BeginReplace{});
}

void EditorWindow::run_go_to_line()
{
    send(application::BeginGoToLine{});
}

// 元に戻す・やり直すと貼り付けの 2 つ（ADR 0157 の決定 2・3）。貼り付けは Ctrl+V と同じ意図
// ClipboardAction の値を変えるだけで、Vim の NORMAL / VISUAL で何もしないのは controller が決める。
void EditorWindow::run_undo()
{
    send_history(core::HistoryDirection::undo);
}

void EditorWindow::run_redo()
{
    send_history(core::HistoryDirection::redo);
}

void EditorWindow::run_paste_plain()
{
    send(application::ClipboardAction{application::ClipboardOperation::paste_plain});
}

void EditorWindow::run_paste_html()
{
    send(application::ClipboardAction{application::ClipboardOperation::paste_html});
}

// 文字の大きさの 3 つ。変換中は送らないのは adjust_font が守る。
void EditorWindow::run_font_larger()
{
    adjust_font(core::FontSizeAdjustment::increase);
}

void EditorWindow::run_font_smaller()
{
    adjust_font(core::FontSizeAdjustment::decrease);
}

void EditorWindow::run_font_reset()
{
    adjust_font(core::FontSizeAdjustment::reset);
}

// 枠の操作（ADR 0148 の決定 6）。本文の行数と幅は deliver が配置の変わり目で送り直す
// （follow_split）。Markdown のプレビューは枠に置くので、枠の操作に並べる（ADR 0155 の決定 3）。
// エクスプローラーは本文の枠を縮める左の面（ADR 0162 の決定 4・7）で、本文の行数と幅は deliver が
// 面の出し入れの変わり目で送り直す（follow_explorer）。
void EditorWindow::run_split_vertical()
{
    send(application::SplitPane{core::SplitLayout::vertical});
}

void EditorWindow::run_split_horizontal()
{
    send(application::SplitPane{core::SplitLayout::horizontal});
}

void EditorWindow::run_next_pane()
{
    send(application::SwitchPane{});
}

// 枠を閉じるのは操作中の枠（`:q` と `Ctrl-w c` と同じ向き・ADR 0148 の決定 5）で、1 枠ではどちらも
// 何もしない。プレビューの枠へは移れないので、プレビューだけを閉じる（ADR 0155）。
void EditorWindow::run_close_pane()
{
    if (!previewing())
    {
        send(application::SwitchPane{});
    }
    send(application::ClosePane{});
}

void EditorWindow::run_toggle_preview()
{
    send(application::TogglePreview{});
}

// 全面の見せ方の遷移は controller の 1 本（ADR 0155 の改訂 16）で、Esc で前の見せ方へ戻る。
void EditorWindow::run_preview_full()
{
    send(application::SetPreviewPlacement{application::PreviewPlacement::full});
}

void EditorWindow::run_toggle_explorer()
{
    send(application::ToggleExplorer{});
}

// 文書への操作。開く・保存は open_document / save_document / save_document_as を表が直に引く。
// 日時の挿入で何を入れるか・入れずに知らせるかは controller の 1 か所が決める
// （ADR 0175 の決定 3）。
void EditorWindow::run_insert_date_time()
{
    send(application::InsertDateTime{});
}

// 保存の形の面はステータスバーのクリックと同じ意図（ADR 0010 の追補 #1049 の決定 2）。
void EditorWindow::run_choose_encoding()
{
    send(application::OpenDocumentFormatList{core::DocumentFormatKind::encoding});
}

void EditorWindow::run_choose_line_ending()
{
    send(application::OpenDocumentFormatList{core::DocumentFormatKind::line_ending});
}

// 本文と字体を写して印刷の port を呼ぶか・知らせるかは controller の 1 か所が決める
// （ADR 0177 の決定 1・2）。
void EditorWindow::run_print()
{
    send(application::PrintDocument{});
}

// 操作「ルートのフォルダを選ぶ」（ADR 0162 の決定 7）。FOS_PICKFOLDERS のダイアログで選んだ
// フォルダを意図にする。取り消しは何も送らない。
void EditorWindow::choose_explorer_root()
{
    auto folder = choose_folder(window_);
    if (folder.has_value())
    {
        send(application::ExplorerRootChosen{std::move(folder).value()});
    }
}

// 保存の失敗の知らせは deliver が AdjustFontSize の後に出す。変換中は送らない（今の鍵と同じ）。
void EditorWindow::adjust_font(core::FontSizeAdjustment adjustment)
{
    if (!controller_.is_composing())
    {
        send(application::AdjustFontSize{adjustment, 1});
    }
}

// ステータスバーのトグルのクリックと同じ意図で、いまの反対を選ぶ。鍵（Ctrl+Shift+Space・D61）も
// 一覧もここを通る。変換中は送らない（IME の変換中は効かない・文字の大きさの鍵と同じ守り）。
void EditorWindow::toggle_mode()
{
    if (controller_.is_composing())
    {
        return;
    }
    switch (mode_)
    {
    case core::EditMode::ordinary:
        send(application::SelectEditMode{core::EditMode::vim});
        return;
    case core::EditMode::vim:
        send(application::SelectEditMode{core::EditMode::ordinary});
        return;
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
