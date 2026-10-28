#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandPalette.hpp"
#include "DisplayText.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExResult.hpp"
#include "FileBookmarkEdit.hpp"
#include "FileFolder.hpp"
#include "FolderRequest.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "SaveState.hpp"
#include "TabTitle.hpp"
#include "UnlistedExtensions.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
using detail::composition_view_of;
using detail::tab_views;

bool EditorController::command_palette_active() const noexcept
{
    const auto &input = state_.command_input();
    return input.has_value() && std::holds_alternative<core::CommandPalette>(input.value());
}

std::optional<CommandPaletteView> EditorController::command_palette_view() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return std::nullopt;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        // 記号の案内は入力が空で、入力行が変換中でないときだけ（ADR 0060 の決定 9）。
        // 1 文字でも打てば消え、変換を始めても消える（長い変換の文字列に重ねない）。
        auto hint = palette->input().text().empty() && !command_composed().has_value()
                        ? std::optional<core::DisplayText>{core::palette_mark_hint()}
                        : std::nullopt;
        // 載せるのは見えている行の窓だけ（ADR 0062 の決定 2）。全件は写さない。窓の先頭は面の
        // 状態（ADR 0150 の決定 1）。
        const auto first = palette->first();
        return CommandPaletteView{palette->rows(first, palette->visible()), first,
                                  palette->selected(), palette->count(), std::move(hint)};
    }
    return std::nullopt;
}

void EditorController::accept(const ScrollPaletteList &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        state_ = state_.with_command_input(palette->scrolled(intent.lines));
    }
}

void EditorController::accept(const OpenCommandPalette &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    // 開いている面の Ctrl+P は前の候補（zsh / Vim と同じ）。閉じるのは Esc（ADR 0150 の決定 2）。
    if (command_palette_active())
    {
        accept(EditCommand{core::CommandEdit::complete_previous});
        return;
    }
    open_palette("", 0);
}

// 開いている間の OpenTabList は閉じる（ADR 0057 の決定 7）。
void EditorController::accept(const OpenTabList &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    if (command_palette_active())
    {
        accept(CancelCommand{});
        return;
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::tabs, {}});
}

// 開いている入力行（検索の preview も）は取消と同じ道で閉じてから開く（ADR 0078 の決定 9）。
void EditorController::accept(const OpenOperationList &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    if (command_line_active())
    {
        accept(CancelCommand{});
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::operations, {}});
}

// 面を開く（ADR 0062 の決定 14）。前の面のときに届いて残った分は collect で捨てる（たまりが空で
// ないと次の列挙の合図が来ない）。券を進めてから頼むので、前の面の分は券で見分けて捨てられる。
void EditorController::open_palette(std::string_view input, std::size_t selected)
{
    static_cast<void>(ports_.folders.collect());
    const auto bookmarks = ports_.bookmarks.read();
    auto entries = palette_entries(bookmarks.value_or(FileBookmarks{}));
    request_folder(entries);
    state_ = state_.with_command_input(
        core::CommandPalette::opened(
            std::move(entries), input, state_.mode(),
            core::CommandLine::empty(state_.themes()).with_fonts(given_fonts()))
            .selected_at(selected));
    if (!bookmarks)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("ブックマークを読めませんでした").value());
    }
}

void EditorController::request_folder(const std::vector<core::CommandChoice> &entries)
{
    palette_folder_ = PaletteFolder{palette_folder_.ticket + 1, {}, 0, false};
    const auto &path = state_.document().path;
    if (!path.has_value())
    {
        return;
    }
    const auto folder = core::folder_of(path.value());
    if (!folder.has_value())
    {
        return;
    }
    palette_folder_.listed = listed_in_folder(folder.value(), entries);
    ports_.folders.list(FolderRequest{folder.value(), palette_folder_.ticket});
}

// 面を開いている間はタブも履歴も変わらないので、開くときに 1 回だけ作る（ADR 0062 の決定 15）。
// 比べるのはファイルのあるフォルダと頼んだフォルダで、比べ方は FilePort が OS の規則で決める。
std::vector<core::FilePath>
EditorController::listed_in_folder(const core::FilePath &folder,
                                   const std::vector<core::CommandChoice> &entries) const
{
    std::vector<core::FilePath> listed;
    // path は optional か expected（どちらも値が無ければ数えない）。
    const auto keep = [&](const auto &path)
    {
        if (path.has_value() && in_folder(path.value(), folder))
        {
            listed.push_back(path.value());
        }
    };
    for (const core::CommandChoice &entry : entries)
    {
        if (entry.kind == core::CommandChoiceKind::open)
        {
            keep(core::FilePath::parse(entry.command));
        }
    }
    return listed;
}

bool EditorController::in_folder(const core::FilePath &path, const core::FilePath &folder) const
{
    const auto home = core::folder_of(path);
    return home.has_value() && ports_.files.same_file(home.value(), folder);
}

// 名前と場所は履歴の候補と同じ作り方、確定はパスの文字列（open_listed へ写す）。
void EditorController::append_folder_choices(const std::vector<core::FilePath> &files,
                                             std::vector<core::CommandChoice> &choices) const
{
    for (const core::FilePath &file : files)
    {
        if (!core::folder_lists(file.file_name()) ||
            std::ranges::any_of(palette_folder_.listed, [&](const core::FilePath &shown)
                                { return ports_.files.same_file(shown, file); }))
        {
            continue;
        }
        choices.push_back(core::CommandChoice{core::tab_title_for(file, core::SaveState::saved),
                                              std::string(file.text()),
                                              core::CommandChoiceKind::open,
                                              core::tab_folder_for(file),
                                              core::PaletteOrigin::folder,
                                              std::nullopt,
                                              {}});
    }
}

// ファイルの候補はパスを持つopenの1本。無題だけはパスが無いのでtabnextを使う（ADR 0063）。
std::vector<core::CommandChoice>
EditorController::palette_tabs(const FileBookmarks &bookmarks) const
{
    const auto views = tab_views(state_, active_document_view());
    std::vector<core::CommandChoice> entries;
    entries.reserve(views.size() + bookmarks.files.size());
    for (std::size_t index = 0; index < views.size(); ++index)
    {
        const DocumentView &view = views.at(index);
        const bool named = view.path.has_value();
        const bool marked = named && bookmarks_contain(bookmarks, view.path.value(), ports_.files);
        entries.push_back(core::CommandChoice{
            view.title,
            named ? std::string(view.path.value().text()) : "tabnext " + std::to_string(index + 1),
            named ? core::CommandChoiceKind::open : core::CommandChoiceKind::execute,
            core::tab_folder_for(view.path),
            marked ? core::PaletteOrigin::bookmarked_tab : core::PaletteOrigin::tab,
            std::nullopt,
            {}});
    }
    return entries;
}

void EditorController::append_bookmark_choices(const FileBookmarks &bookmarks,
                                               std::vector<core::CommandChoice> &entries) const
{
    FileBookmarks listed;
    for (const core::FilePath &path : bookmarks.files)
    {
        if (!open_tab_of(path).has_value() && !bookmarks_contain(listed, path, ports_.files))
        {
            entries.push_back(core::CommandChoice{core::tab_title_for(path, core::SaveState::saved),
                                                  std::string(path.text()),
                                                  core::CommandChoiceKind::open,
                                                  core::tab_folder_for(path),
                                                  core::PaletteOrigin::bookmark,
                                                  std::nullopt,
                                                  {}});
            listed.files.push_back(path);
        }
    }
}

std::vector<core::CommandChoice>
EditorController::palette_entries(const FileBookmarks &bookmarks) const
{
    auto entries = palette_tabs(bookmarks);
    append_bookmark_choices(bookmarks, entries);
    const auto history = ports_.history.read();
    if (!history)
    {
        return entries;
    }
    for (const core::FilePath &path : history.value().files)
    {
        if (!open_tab_of(path).has_value() && !bookmarks_contain(bookmarks, path, ports_.files))
        {
            entries.push_back(core::CommandChoice{core::tab_title_for(path, core::SaveState::saved),
                                                  std::string(path.text()),
                                                  core::CommandChoiceKind::open,
                                                  core::tab_folder_for(path),
                                                  core::PaletteOrigin::history,
                                                  std::nullopt,
                                                  {}});
        }
    }
    return entries;
}

// 保存済みの文書か、面で選んだファイルだけ。設定/Ex/検索の文字列をパスとして解釈しない。
std::optional<core::FilePath> EditorController::bookmark_target() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return state_.document().path;
    }
    const auto *palette = std::get_if<core::CommandPalette>(&input.value());
    if (palette == nullptr)
    {
        return std::nullopt;
    }
    const auto choice = palette->choice_at(palette->selected());
    if (!choice.has_value() || choice.value().kind != core::CommandChoiceKind::open)
    {
        return std::nullopt;
    }
    const auto path = core::FilePath::parse(choice.value().command);
    return path.has_value() ? std::optional{path.value()} : std::nullopt;
}

bool EditorController::bookmark_toggle_allowed() const
{
    if (state_.composition().has_value() || (command_line_active() && !command_palette_active()))
    {
        return false;
    }
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return true;
    }
    const auto *palette = std::get_if<core::CommandPalette>(&input.value());
    // 設定のコマンド（:）と操作の一覧（?）の面にはファイルが無いので、何も出さず何もしない。
    return palette != nullptr && palette->scope() != core::PaletteScope::commands &&
           palette->scope() != core::PaletteScope::operations;
}

void EditorController::accept(const ToggleBookmark &)
{
    if (!bookmark_toggle_allowed())
    {
        return;
    }
    const auto path = bookmark_target();
    if (!path.has_value())
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("保存済みのファイルを選んでください").value());
        return;
    }
    const auto before = ports_.bookmarks.read();
    if (!before)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("ブックマークを読めませんでした").value());
        return;
    }
    const bool removing = bookmarks_contain(before.value(), path.value(), ports_.files);
    const auto after = bookmarks_toggled(before.value(), path.value(), ports_.files);
    if (!after)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("ブックマークは " + std::to_string(bookmark_limit) +
                                     " 件まで登録できます")
                .value());
        return;
    }
    if (!ports_.bookmarks.write(after.value()))
    {
        reject(core::DisplayText::parse("ブックマークを保存できませんでした").value());
        return;
    }
    close_command_input();
    state_ = state_.with_command_message(
        core::DisplayText::parse(removing ? "ブックマークを解除しました"
                                          : "ブックマークに追加しました")
            .value());
}

void EditorController::accept(const ActivateCommandChoice &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        if (intent.index < palette->count())
        {
            submit_palette(palette->selected_at(intent.index));
        }
    }
}

void EditorController::submit_palette(const core::CommandPalette &palette)
{
    const auto chosen = palette.choice_at(palette.selected());
    if (!chosen.has_value())
    {
        return;
    }
    const auto &choice = chosen.value();
    switch (choice.kind)
    {
    case core::CommandChoiceKind::execute:
        evaluate_command(choice.command);
        return;
    case core::CommandChoiceKind::open:
        // 入力を閉じてから開く（evaluate_command と同じ順・choice は入力ではなく手元の列を指す）。
        // open の候補は palette_entries が検証済みの FilePath の text() から作るので parse は必ず
        // 成功する（不変条件）。失敗したら何もしない。
        close_command_input();
        if (const auto path = core::FilePath::parse(choice.command); path.has_value())
        {
            open_listed(path.value(), choice.origin);
        }
        return;
    case core::CommandChoiceKind::operate:
        // 面を閉じて頼まれた操作を 1 意図だけ立てる。実行は ui の run_operation で、controller は
        // 操作ごとの分岐を持たない（ADR 0078 の決定 7・8）。
        close_command_input();
        state_ = state_.with_operation_request(choice.operation);
        return;
    case core::CommandChoiceKind::fill:
        break;
    }
    auto next = palette.filled(choice.command);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    state_ = state_.with_command_input(std::move(next).value());
    reach_font_families();
}

std::optional<CompositionView> EditorController::command_composed() const
{
    if (!command_line_active())
    {
        return std::nullopt;
    }
    return composition_view_of(state_.composition());
}
} // namespace nenenib::application
