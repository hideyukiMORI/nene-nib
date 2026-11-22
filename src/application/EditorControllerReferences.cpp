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

// 参照の当たり（text は開いているタブの本文・開いていなければ nullptr）。
[[nodiscard]] core::ProjectHit reference_hit(const core::LspLocation &at,
                                             const core::TextBuffer *text,
                                             core::LspPositionEncoding encoding)
{
    return text != nullptr ? opened_hit(*text, at, encoding) : unopened_hit(at);
}
} // namespace

// 参照の答え（決定 4）は 1 回使って消す。何も待っていないときだけ一覧を開き、空なら 1 行知らせる。
// 違えば知らせずに捨てる（定義の答えと同じ・ADR 0190 の追補 3）。窓が非アクティブの間に届いた答えは
// 一覧を開かずに控える（`&` で開けば見える・#1954 のレビュー R5）。切った答えは「N 件のうち」を
// 1 行知らせる（L2）。
void EditorController::settle_references_answer()
{
    auto answers = state_.language_answers();
    if (!answers.references.has_value())
    {
        return;
    }
    LanguageDefinition answered = std::move(answers.references).value();
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
    const std::size_t listed = answered.locations.size();
    const std::size_t total = answered.total;
    held_references_ = std::move(answered);
    if (window_inactive_)
    {
        return;
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::references, {}});
    if (total > listed)
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_references_first_of,
                                                  {std::to_string(listed), std::to_string(total)}));
    }
}

// 控えた答えを捨てる（#1954 のレビュー R1）。答えを頼んだ文書を言語サーバーが閉じたとき（タブを
// 閉じた・lsp をオフにした・席が止まった）に、文書を閉じる道の 1 か所から呼ぶ。
void EditorController::forget_listed_answers(AutosaveId document)
{
    if (held_references_.has_value() && held_references_.value().context.document == document)
    {
        held_references_.reset();
    }
}

// 面を開くときに 1 回だけ作る参照の候補（ADR 0062 の決定 15）。答えの順のまま。`file:` でない場所は
// 外す。ルートは 1 回、ファイル・タブ・本文は URI ごとに 1 回だけ引く（#1954 のレビュー R1・1,000
// 件 × タブ 20 で 1 件ごとに引くと Debug で 1 回 約 70 ms）。
void EditorController::append_reference_choices(std::vector<core::CommandChoice> &entries)
{
    palette_references_.clear();
    if (!held_references_.has_value())
    {
        return;
    }
    const auto encoding = held_references_.value().encoding;
    const auto root = project_search_root();
    std::vector<std::pair<std::string_view, ListedFile>> seen;
    for (const core::LspLocation &location : held_references_.value().locations)
    {
        const ListedFile &listed = listed_file_in(seen, location.uri);
        if (!listed.first.has_value())
        {
            continue;
        }
        const core::FilePath &file = listed.first.value();
        const core::ProjectHit hit = reference_hit(location, listed.second, encoding);
        core::CommandChoice choice =
            core::project_hit_choice(file, core::project_hit_row(root.value_or(file), file, hit));
        choice.command = std::to_string(palette_references_.size());
        choice.kind = core::CommandChoiceKind::land;
        choice.origin = core::PaletteOrigin::reference;
        entries.push_back(std::move(choice));
        palette_references_.push_back(
            PaletteReference{file, LspLanding{.at = location.range.start, .encoding = encoding},
                             PreviewAnchor{hit.line, hit.match_begin, hit.match_end}});
    }
}

// 引いた URI の覚え seen から引き、無ければ引いて覚える（URI の数は答えのファイルの数）。
const EditorController::ListedFile &
EditorController::listed_file_in(std::vector<std::pair<std::string_view, ListedFile>> &seen,
                                 const std::string &uri) const
{
    const auto found = std::ranges::find(seen, std::string_view{uri},
                                         &std::pair<std::string_view, ListedFile>::first);
    if (found != seen.end())
    {
        return found->second;
    }
    seen.emplace_back(uri, listed_file_of(uri));
    return seen.back().second;
}

// URI → ファイルと、開いているタブの本文（開いていなければ nullptr・`file:` でなければ値なし）。
EditorController::ListedFile EditorController::listed_file_of(const std::string &uri) const
{
    auto file = core::file_path_of_uri(uri);
    const auto tab = file.has_value() ? open_tab_of(file.value()) : std::nullopt;
    const auto text = tab.has_value() ? tab_text_at(tab.value()) : std::nullopt;
    return ListedFile{std::move(file), text.has_value() ? text.value().first : nullptr};
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
