#include "EditorController.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandPalette.hpp"
#include "DisplayText.hpp"
#include "EditorControllerDetail.hpp"
#include "FilePath.hpp"
#include "FinderPreview.hpp"
#include "FinderPreviews.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "PaletteLayout.hpp"
#include "PaletteOrigin.hpp"
#include "PreviewAnchor.hpp"
#include "PreviewBatch.hpp"
#include "PreviewFailure.hpp"
#include "PreviewKey.hpp"
#include "PreviewLines.hpp"
#include "PreviewRequest.hpp"
#include "PreviewSnippet.hpp"
#include "SaveState.hpp"
#include "TabTitle.hpp"
#include "UiMessage.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

// 探して見る面のプレビュー欄の群（ADR 0189 の段 P1・P2）。置き方の切り替えと、選んでいる候補の
// 表示値と、開いていないファイルの窓の要求・券・保持。開いているタブの文書は手元の本文から作り、
// port を呼ばない。
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

// 寸法が届く前に描いている扱いの形（automatic は right・ほかは選んだ値）。
[[nodiscard]] core::FinderPreview unmeasured_drawn(core::FinderPreview chosen) noexcept
{
    return chosen == core::FinderPreview::automatic ? core::FinderPreview::right : chosen;
}

// 見出し（名前・場所・言語）だけの表示値。パスがあればファイルの名前と場所とパスの言語、無題は
// タブの題名だけ（言語は本文の文書の値で後から置き換える）。
// 作れない理由の文（3 列）。
[[nodiscard]] core::UiMessage preview_failure_message(PreviewFailure failure) noexcept
{
    switch (failure)
    {
    case PreviewFailure::too_large:
        return core::UiMessage::palette_preview_too_large;
    case PreviewFailure::binary:
        return core::UiMessage::palette_preview_binary;
    case PreviewFailure::unreadable:
        return core::UiMessage::palette_preview_unreadable;
    case PreviewFailure::not_local:
        return core::UiMessage::palette_preview_not_local;
    }
    std::unreachable();
}

// 保持の鍵（パスの文字列と窓の中心の行）。
[[nodiscard]] PreviewKey preview_key_of(const core::FilePath &path,
                                        const std::optional<PreviewAnchor> &anchor)
{
    return PreviewKey{std::string(path.text()),
                      anchor.transform([](const PreviewAnchor &at) { return at.line.value; })};
}

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

bool EditorController::preview_drawn(core::FinderPreview placement) const noexcept
{
    if (placement == core::FinderPreview::off)
    {
        return false;
    }
    const bool known = window_width_ > 0 && window_height_ > 0 && view_geometry_.dpi > 0;
    return !known || core::palette_preview_drawn(placement, window_width_, window_height_,
                                                 view_geometry_.dpi) != core::FinderPreview::off;
}

// 今描いている形の次（決定 3・選んだ値に依らない）。描いている形は窓の大きさで決まり、出どころには
// 依らない（欄を持たない出どころで押しても、覚える値は窓に入る形の次）。寸法が届く前は選んだ値
// （automatic なら right）を描いている扱い。
void EditorController::accept(const CycleFinderPreview &)
{
    const auto chosen = state_.settings().finder_preview;
    const bool known = window_width_ > 0 && window_height_ > 0 && view_geometry_.dpi > 0;
    const auto drawn = known ? core::palette_preview_drawn(chosen, window_width_, window_height_,
                                                           view_geometry_.dpi)
                             : unmeasured_drawn(chosen);
    auto settings = state_.settings();
    settings.finder_preview = core::finder_preview_cycled(drawn);
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
    case core::CommandChoiceKind::land:
        // 診断の候補は開いているタブの文書（手元の本文から窓を作る・#1939）。
        if (const auto target = palette_diagnostic_of(choice); target.has_value())
        {
            const auto views = detail::tab_views(state_, active_document_view());
            return {views.at(target.value().tab).path, target.value().tab};
        }
        // 参照の候補はファイルと、同じファイルを開いているタブ（#1947）。
        if (const auto target = palette_reference_of(choice); target.has_value())
        {
            return {target.value().file, open_tab_of(target.value().file)};
        }
        return {};
    case core::CommandChoiceKind::fill:
    case core::CommandChoiceKind::operate:
        return {};
    }
    std::unreachable();
}

// 全文検索の当たりは面の index 番目の当たり（open_project_hit と同じ対応）、診断は控えた候補の
// 印（#1939）。ほかの候補は無し。
std::optional<PreviewAnchor>
EditorController::preview_anchor_of(const core::CommandPalette &palette,
                                    const core::CommandChoice &choice) const
{
    if (const auto target = palette_diagnostic_of(choice); target.has_value())
    {
        return target.value().anchor;
    }
    if (const auto target = palette_reference_of(choice); target.has_value())
    {
        return target.value().anchor;
    }
    if (choice.origin != core::PaletteOrigin::project_hit ||
        palette.selected() >= project_search_.hits.size())
    {
        return std::nullopt;
    }
    const auto &hit = project_search_.hits.at(palette.selected()).hit;
    return PreviewAnchor{hit.line, hit.text_begin + hit.match_begin,
                         hit.text_begin + hit.match_end};
}

// 選んでいる候補が port で読む窓を要るなら、その要求（券は 0・決定 4）。開いているタブの文書は
// 手元の本文から作るので要求しない。まだ読んでいないタブはパスで読む（reach_tab を呼ばない）。
std::optional<PreviewRequest>
EditorController::preview_request_of(const core::CommandPalette &palette) const
{
    const auto chosen = palette.choice_at(palette.selected());
    if (!core::palette_previews(palette.scope()) || !chosen.has_value())
    {
        return std::nullopt;
    }
    auto [path, position] = preview_target(chosen.value());
    if (!path.has_value() || (position.has_value() && tab_text_at(position.value()).has_value()))
    {
        return std::nullopt;
    }
    const auto language = core::language_of_path(path.value().text());
    return PreviewRequest{std::move(path).value(), preview_anchor_of(palette, chosen.value()),
                          language, 0};
}

// 見出しは選んだ候補のもの（決定 4）。中身は開いているタブの文書なら手元の本文から作り、ほかは
// 届いた窓（直近 finder_preview_held 個の保持）か、届くまで空。
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
    const auto anchor = preview_anchor_of(palette, choice);
    if (anchor.has_value())
    {
        snippet.line = ui_notice(core::UiMessage::palette_preview_line,
                                 {std::to_string(anchor.value().line.value)});
    }
    const auto text = position.has_value() ? tab_text_at(position.value()) : std::nullopt;
    if (text.has_value())
    {
        snippet.language = std::string(core::language_name(text.value().second));
        snippet.lines = preview_lines(*text.value().first, anchor, text.value().second);
        return snippet;
    }
    if (path.has_value())
    {
        held_preview(preview_key_of(path.value(), anchor), snippet);
        // 保持の窓の本文と色はそのまま、印は今の当たりから付け直す（語を打ち足して同じ行の範囲が
        // 変わっても前の印を残さない・V-1794 の必須 2）。
        marked_lines(snippet.lines, anchor);
    }
    return snippet;
}

// 保持に鍵の窓があれば snippet へ写す（作れない理由は 1 行の文）。無ければ空のまま（届くまで）。
void EditorController::held_preview(const PreviewKey &key, PreviewSnippet &snippet) const
{
    const auto found = std::ranges::find_if(previews_.held, [&key](const auto &entry)
                                            { return entry.first == key; });
    if (found == previews_.held.end())
    {
        return;
    }
    if (found->second.has_value())
    {
        snippet.lines = found->second.value();
        return;
    }
    snippet.notice = ui_notice(preview_failure_message(found->second.error()));
}

// 意図の出口の 1 本（決定 4）。面が閉じたら今の要求を取り消して保持を全部捨てる。面が開いていて
// 欄を描くなら、選んでいる候補の窓が保持にも今の要求にも無いときだけ、前の券を取り消して新しい券で
// 頼む。面が閉じていて何も持っていなければ欄を読むだけで抜ける。
void EditorController::settle_finder_preview()
{
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    if (palette == nullptr)
    {
        forget_finder_previews();
        return;
    }
    if (!preview_drawn(core::palette_placement(palette->scope(), state_.settings().finder_preview)))
    {
        return;
    }
    auto wanted = preview_request_of(*palette);
    if (!wanted.has_value())
    {
        return;
    }
    auto key = preview_key_of(wanted.value().path, wanted.value().anchor);
    const bool held = std::ranges::any_of(previews_.held,
                                          [&key](const auto &entry) { return entry.first == key; });
    if (held || (previews_.live != 0 && previews_.waiting == key))
    {
        return;
    }
    if (previews_.live != 0)
    {
        ports_.previews.cancel(previews_.live);
    }
    wanted.value().ticket = ++previews_.tickets;
    previews_.live = wanted.value().ticket;
    previews_.draining = true;
    previews_.waiting = std::move(key);
    ports_.previews.request(std::move(wanted).value());
}

// 面を閉じた（決定 4）。今の要求を取り消して保持を全部捨てる。何も持っていなければ何もしない。
void EditorController::forget_finder_previews()
{
    if (previews_.live != 0)
    {
        ports_.previews.cancel(previews_.live);
    }
    if (previews_.live != 0 || !previews_.held.empty())
    {
        previews_ = FinderPreviews{previews_.tickets, 0, std::nullopt, {}, previews_.draining};
    }
}

// WorkCompleted の 1 本（決定 4）。頼んだ後に棚が空と確かめるまで collect し（面を閉じた後も
// 届いていた batch を捨てる）、頼んでいなければ port を呼ばない。今の券の窓だけを保持に足し、
// 古い券の結果は捨てる。保持は古いものから捨てて finder_preview_held 個まで。
void EditorController::collect_finder_preview()
{
    if (!previews_.draining)
    {
        return;
    }
    auto batches = ports_.previews.collect();
    if (batches.empty() && previews_.live == 0)
    {
        previews_.draining = false;
        return;
    }
    for (PreviewBatch &batch : batches)
    {
        if (batch.ticket != previews_.live || !previews_.waiting.has_value())
        {
            continue;
        }
        previews_.held.emplace_back(std::move(previews_.waiting).value(), std::move(batch.lines));
        if (previews_.held.size() > finder_preview_held)
        {
            previews_.held.erase(previews_.held.begin());
        }
        previews_.live = 0;
        previews_.waiting.reset();
    }
}
} // namespace nenenib::application
