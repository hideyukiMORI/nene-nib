#include "EditorController.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandPalette.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "FinderPreview.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "PaletteLayout.hpp"
#include "PaletteOrigin.hpp"
#include "PreviewAnchor.hpp"
#include "PreviewLines.hpp"
#include "PreviewSnippet.hpp"
#include "SaveState.hpp"
#include "TabTitle.hpp"
#include "UiMessage.hpp"

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

// 探して見る面のプレビュー欄の群（ADR 0189 の段 P1）。置き方の切り替えと、選んでいる候補の表示値。
// 中身を作るのは開いているタブの文書だけ（手元の本文・port を呼ばない）。ファイルを読む PreviewPort
// は段 P2。
namespace nenenib::application
{
namespace
{
// 無題のタブの候補の確定 `tabnext N`（palette_tabs が作る形）の帯の位置（0 始まり）。
[[nodiscard]] std::optional<std::size_t> untitled_position(std::string_view command) noexcept
{
    constexpr std::string_view head = "tabnext ";
    if (!command.starts_with(head))
    {
        return std::nullopt;
    }
    const std::string_view digits = command.substr(head.size());
    std::size_t number = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || number == 0)
    {
        return std::nullopt;
    }
    return number - 1;
}

// 見出し（名前・場所・言語）だけの表示値。パスがあればファイルの名前と場所とパスの言語、無題は
// タブの題名だけ（言語は本文の文書の値で後から置き換える）。
[[nodiscard]] PreviewSnippet heading_of(const core::CommandChoice &choice,
                                        const std::optional<core::FilePath> &path)
{
    if (!path.has_value())
    {
        return PreviewSnippet{choice.label, std::nullopt,
                              std::nullopt, std::string(core::language_name(core::Language::plain)),
                              {},           std::nullopt};
    }
    return PreviewSnippet{
        core::tab_title_for(path.value(), core::SaveState::saved),
        core::tab_folder_for(path),
        std::nullopt,
        std::string(core::language_name(core::language_of_path(path.value().text()))),
        {},
        std::nullopt};
}
} // namespace

core::FinderPreview EditorController::palette_placement() const noexcept
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return core::FinderPreview::off;
    }
    const auto *palette = std::get_if<core::CommandPalette>(&input.value());
    return palette == nullptr
               ? core::FinderPreview::off
               : core::palette_placement(palette->scope(), state_.settings().finder_preview);
}

// 今 automatic なら今描いている形の次（決定 3）。描いている形は窓の大きさで決まり、出どころには
// 依らない（欄を持たない出どころで押しても、覚える値は窓に入る形の次）。寸法が届く前は right を
// 描いている扱い。
void EditorController::accept(const CycleFinderPreview &)
{
    const auto chosen = state_.settings().finder_preview;
    const bool known = window_width_ > 0 && window_height_ > 0 && view_geometry_.dpi > 0;
    const auto drawn = known ? core::palette_preview_drawn(chosen, window_width_, window_height_,
                                                           view_geometry_.dpi)
                             : core::FinderPreview::right;
    auto settings = state_.settings();
    settings.finder_preview = core::finder_preview_cycled(chosen, drawn);
    static_cast<void>(persist_settings(std::move(settings)));
}

std::optional<std::pair<const core::TextBuffer *, core::Language>>
EditorController::tab_text_at(std::size_t position) const
{
    if (position == state_.active_tab())
    {
        return std::pair{&state_.text(), state_.language()};
    }
    const DocumentState *bundle = state_.parked_bundle_at(position);
    if (bundle == nullptr)
    {
        return std::nullopt;
    }
    return std::pair{&bundle->text, bundle->language};
}

// 候補が指すファイルと帯の位置。ファイルの候補（タブ・履歴・同じフォルダ・全文検索の当たり）は
// パスと、同じファイルを開いているタブの位置。無題のタブは `tabnext N` の位置だけ。ほかは両方なし。
std::pair<std::optional<core::FilePath>, std::optional<std::size_t>>
EditorController::preview_target(const core::CommandChoice &choice) const
{
    switch (choice.kind)
    {
    case core::CommandChoiceKind::open:
        if (auto parsed = core::FilePath::parse(choice.command); parsed.has_value())
        {
            const auto position = open_tab_of(parsed.value());
            return {std::move(parsed).value(), position};
        }
        return {};
    case core::CommandChoiceKind::execute:
        return {std::nullopt, untitled_position(choice.command)};
    case core::CommandChoiceKind::fill:
    case core::CommandChoiceKind::operate:
        return {};
    }
    std::unreachable();
}

// 見出しは選んだ候補のもの（決定 4）。中身は開いているタブの文書だけ手元の本文から作り、まだ読んで
// いない文書・開いていないファイルは見出しと 1 行の知らせ（段 P2 で worker が読む）。
std::optional<PreviewSnippet>
EditorController::preview_snippet(const core::CommandPalette &palette) const
{
    const auto chosen = palette.choice_at(palette.selected());
    if (!core::palette_previews(palette.scope()) || !chosen.has_value())
    {
        return std::nullopt;
    }
    const core::CommandChoice &choice = chosen.value();
    const auto [path, position] = preview_target(choice);
    if (!path.has_value() && !position.has_value())
    {
        return std::nullopt;
    }
    PreviewSnippet snippet = heading_of(choice, path);
    // 全文検索の当たりは面の index 番目の当たり（open_project_hit と同じ対応）。
    std::optional<PreviewAnchor> anchor;
    if (choice.origin == core::PaletteOrigin::project_hit &&
        palette.selected() < project_search_.hits.size())
    {
        const auto &hit = project_search_.hits.at(palette.selected()).hit;
        anchor = PreviewAnchor{hit.line, hit.text_begin + hit.match_begin,
                               hit.text_begin + hit.match_end};
        snippet.line =
            ui_notice(core::UiMessage::palette_preview_line, {std::to_string(hit.line.value)});
    }
    const auto text = position.has_value() ? tab_text_at(position.value()) : std::nullopt;
    if (!text.has_value())
    {
        snippet.notice = ui_notice(core::UiMessage::palette_preview_unread);
        return snippet;
    }
    snippet.language = std::string(core::language_name(text.value().second));
    snippet.lines = preview_lines(*text.value().first, anchor);
    return snippet;
}
} // namespace nenenib::application
