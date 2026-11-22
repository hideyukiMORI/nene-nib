#include "EditorController.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "ExPaletteRequest.hpp"
#include "FileUri.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageDefinition.hpp"
#include "LspLanding.hpp"
#include "LspLocation.hpp"
#include "LspPosition.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteReference.hpp"
#include "PaletteScope.hpp"
#include "PreviewAnchor.hpp"
#include "ProjectHit.hpp"
#include "ProjectHitRows.hpp"
#include "ProjectSearchRules.hpp"
#include "UiMessage.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// 言語サーバーの参照の一覧（#1947・ADR 0191 の決定 4）。答えは Ctrl+P の一覧の記号 `&` の候補に
// なり、選ぶと定義と同じ着地の 1 本で跳ぶ。開いているタブの候補は本文から行の文を出し、開いていない
// ファイルは行の文を読まない（プレビュー欄が既存の PreviewPort で見せる）。
namespace nenenib::application
{
namespace
{
// 一覧に出す参照の上限（決定 4）。
constexpr std::size_t reference_limit = 1000;

// 候補の command（控えた列の位置の 10 進）→ 位置。
[[nodiscard]] std::optional<std::size_t> listed_index(std::string_view command) noexcept
{
    std::size_t index = 0;
    const auto parsed = std::from_chars(command.data(), command.data() + command.size(), index);
    if (parsed.ec != std::errc{} || parsed.ptr != command.data() + command.size())
    {
        return std::nullopt;
    }
    return index;
}

// 開いている本文の中の当たり（全文検索の当たりと同じ形・行の文は行頭から最大
// project_hit_text_bytes バイト・code point を割らない）。
[[nodiscard]] core::ProjectHit opened_hit(const core::TextBuffer &text, const core::LspLocation &at,
                                          core::LspPositionEncoding encoding)
{
    const core::Offset end_of_text{text.size_bytes()};
    const core::Offset begin =
        core::lsp_offset_of(text, at.range.start, encoding).value_or(end_of_text);
    const core::Offset end = core::lsp_offset_of(text, at.range.end, encoding).value_or(begin);
    const core::TextPosition position = text.position_of(begin);
    const std::size_t start = text.line_start(position.line).value;
    const std::size_t line_end = text.line_end(position.line).value;
    const std::size_t window_end = std::min(line_end, start + core::project_hit_text_bytes);
    std::string line = text.text_range(core::Offset{start}, core::Offset{window_end});
    line.resize(core::whole_code_points(line));
    const std::size_t match_begin = std::min(begin.value - start, line.size());
    // 終わりは始まりの行の中に切る（行をまたぐ範囲は行末まで）。
    const std::size_t match_end = std::clamp(
        std::min(std::max(end.value, begin.value), line_end) - start, match_begin, line.size());
    return core::ProjectHit{position.line, position.column, std::move(line), 0,
                            match_begin,   match_end};
}

// 開いていないファイルの当たり。行は答えの行、桁は答えの文字の位置 + 1（ASCII の行では同じ）。
// 行の文は読まない。
[[nodiscard]] core::ProjectHit unopened_hit(const core::LspLocation &at)
{
    return core::ProjectHit{core::LineNumber{at.range.start.line + 1},
                            core::Column{at.range.start.character + 1},
                            {},
                            0,
                            0,
                            0};
}
} // namespace

// 参照の答え（決定 4）は 1 回使って消す。何も待っていないときだけ一覧を開き、空なら 1 行知らせる。
// 違えば知らせずに捨てる（定義の答えと同じ・ADR 0190 の追補 3）。
void EditorController::settle_references_answer()
{
    const auto &answer = state_.language_answers().references;
    if (!answer.has_value())
    {
        return;
    }
    LanguageDefinition answered = answer.value();
    LanguageAnswers answers = state_.language_answers();
    answers.references.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const bool usable = faces_closed() && awaits_nothing();
    definition_anchor_.reset();
    if (!usable)
    {
        return;
    }
    if (answered.locations.empty())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_references_not_found));
        return;
    }
    if (answered.locations.size() > reference_limit)
    {
        answered.locations.resize(reference_limit);
    }
    held_references_ = std::move(answered);
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::references, {}});
}

// 面を開くときに 1 回だけ作る参照の候補（ADR 0062 の決定 15）。答えの順のまま。`file:` でない場所は
// 外す。
void EditorController::append_reference_choices(std::vector<core::CommandChoice> &entries)
{
    palette_references_.clear();
    if (!held_references_.has_value())
    {
        return;
    }
    const auto encoding = held_references_.value().encoding;
    for (const core::LspLocation &location : held_references_.value().locations)
    {
        const auto file = core::file_path_of_uri(location.uri);
        if (!file.has_value())
        {
            continue;
        }
        const auto tab = open_tab_of(file.value());
        const auto text = tab.has_value() ? tab_text_at(tab.value()) : std::nullopt;
        const core::ProjectHit hit = text.has_value()
                                         ? opened_hit(*text.value().first, location, encoding)
                                         : unopened_hit(location);
        const core::FilePath root = project_search_root().value_or(file.value());
        core::CommandChoice choice =
            core::project_hit_choice(file.value(), core::project_hit_row(root, file.value(), hit));
        choice.command = std::to_string(palette_references_.size());
        choice.kind = core::CommandChoiceKind::land;
        choice.origin = core::PaletteOrigin::reference;
        entries.push_back(std::move(choice));
        palette_references_.push_back(PaletteReference{
            file.value(), LspLanding{.at = location.range.start, .encoding = encoding},
            PreviewAnchor{hit.line, hit.match_begin, hit.match_end}});
    }
}

std::optional<PaletteReference>
EditorController::palette_reference_of(const core::CommandChoice &choice) const
{
    if (choice.kind != core::CommandChoiceKind::land ||
        choice.origin != std::optional{core::PaletteOrigin::reference})
    {
        return std::nullopt;
    }
    const auto index = listed_index(choice.command);
    if (!index.has_value() || index.value() >= palette_references_.size())
    {
        return std::nullopt;
    }
    return palette_references_.at(index.value());
}

// 候補を選ぶ（決定 4）。面を閉じ、定義と同じ着地の 1 本（開いていなければ開いてから・`''`
// を置く）。
void EditorController::land_palette_reference(const core::CommandChoice &choice)
{
    const auto target = palette_reference_of(choice);
    close_command_input();
    if (!target.has_value())
    {
        return;
    }
    land_location(target.value().file, target.value().at);
}
} // namespace nenenib::application
