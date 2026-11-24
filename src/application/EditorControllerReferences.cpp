#include "EditorController.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandPalette.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "ExPaletteRequest.hpp"
#include "FileUri.hpp"
#include "GiantLine.hpp"
#include "KeyChord.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageSymbols.hpp"
#include "LspLanding.hpp"
#include "LspLocation.hpp"
#include "LspPosition.hpp"
#include "LspRange.hpp"
#include "LspRequestContext.hpp"
#include "LspSymbol.hpp"
#include "LspSymbols.hpp"
#include "OperationBindings.hpp"
#include "OperationTexts.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteReference.hpp"
#include "PaletteScope.hpp"
#include "PreviewAnchor.hpp"
#include "ProjectHit.hpp"
#include "ProjectHitRow.hpp"
#include "ProjectHitRows.hpp"
#include "ProjectSearchRules.hpp"
#include "ServerAbilities.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

// 言語サーバーの参照の一覧（#1947・ADR 0191 の決定 4）と記号の一覧（#1955・決定 5）。答えは
// Ctrl+P の一覧の記号 `&` と `%` の候補になり、選ぶと定義と同じ着地の 1 本で跳ぶ。開いているタブの
// 候補は本文から行の文を出し、開いていないファイルは行の文を読まない（プレビュー欄が既存の
// PreviewPort で見せる）。
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
[[nodiscard]] core::ProjectHit opened_hit(const core::TextBuffer &text, const core::LspRange &at,
                                          core::LspPositionEncoding encoding)
{
    const core::Offset end_of_text{text.size_bytes()};
    const core::Offset begin = core::lsp_offset_of(text, at.start, encoding).value_or(end_of_text);
    const core::TextPosition position = text.position_of(begin);
    const std::size_t start = text.line_start(position.line).value;
    const std::size_t line_end = text.line_end(position.line).value;
    // 終わりが別の行なら変換せず行末に切る（終わりの行が巨大な行でも写さない・#1958 の確かめ
    // レビュー L1）。
    const core::Offset end = at.end.line == at.start.line
                                 ? core::lsp_offset_of(text, at.end, encoding).value_or(begin)
                                 : core::Offset{line_end};
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

// 開いていないファイルの当たり。行は答えの行。桁は出さない（答えの文字の位置は UTF-16 などの単位
// で、ASCII でない行では本文の桁とずれる・設計席の決め 2026-11-22）。行の文は読まない。
[[nodiscard]] core::ProjectHit unopened_hit(const core::LspRange &at)
{
    return core::ProjectHit{core::LineNumber{at.start.line + 1}, core::Column{1}, {}, 0, 0, 0};
}

// 答えの行が巨大な行か（バイト数が giant_line_threshold を超える・#1958 のレビュー R1）。巨大な行
// の当たりは行の文を出さず、位置の変換で行頭から行を写さない（行だけの当たり・着地は選んだときに
// 1 回だけ写す）。バイト数はコードポイントの数以上なので、境目の近くの行も行だけの側に倒す。
[[nodiscard]] bool giant_answer_line(const core::TextBuffer &text,
                                     const core::LspRange &at) noexcept
{
    const core::LineNumber line{at.start.line + 1};
    return line.value <= text.line_count() &&
           text.line_end(line).value - text.line_start(line).value > core::giant_line_threshold;
}

// 当たり（text は開いているタブの本文・開いていなければ nullptr）。
[[nodiscard]] core::ProjectHit listed_hit(const core::LspRange &at, const core::TextBuffer *text,
                                          core::LspPositionEncoding encoding)
{
    return text != nullptr && !giant_answer_line(*text, at) ? opened_hit(*text, at, encoding)
                                                            : unopened_hit(at);
}

// Vim の鍵（一覧の空の行の案内に差し込む・ADR 0191 の決定 4・5）。鍵の名前は訳さない。
constexpr std::string_view vim_references_keys = "grr";
constexpr std::string_view vim_symbols_keys = "gO";
constexpr std::string_view vim_code_action_keys = "gra";

// 通常モードの操作の鍵の表示名（鍵が無ければ空）。
[[nodiscard]] std::string chord_label_of(core::EditorOperation operation)
{
    const auto chord = core::shown_chord(operation, core::EditMode::ordinary);
    return chord.has_value() ? core::key_chord_label(chord.value()) : std::string{};
}

// 参照・記号・クイックフィックスの一覧の能力と、席がそれを持たないときの知らせ（ほかの出どころは
// 値なし）。
[[nodiscard]] std::optional<std::pair<bool ServerAbilities::*, core::UiMessage>>
refusal_of(core::PaletteScope scope)
{
    switch (scope)
    {
    case core::PaletteScope::references:
        return std::pair{&ServerAbilities::references,
                         core::UiMessage::notice_references_not_supported};
    case core::PaletteScope::symbols:
        return std::pair{&ServerAbilities::document_symbols,
                         core::UiMessage::notice_symbols_not_supported};
    case core::PaletteScope::code_actions:
        return std::pair{&ServerAbilities::code_action,
                         core::UiMessage::notice_code_action_not_supported};
    case core::PaletteScope::files:
    case core::PaletteScope::tabs:
    case core::PaletteScope::bookmarks:
    case core::PaletteScope::history:
    case core::PaletteScope::folder:
    case core::PaletteScope::commands:
    case core::PaletteScope::operations:
    case core::PaletteScope::project_hits:
    case core::PaletteScope::diagnostics:
        return std::nullopt;
    }
    std::unreachable();
}

// 参照の候補の題名。開いているタブは「相対パス:行:桁」、開いていないファイルは「相対パス:行」。
[[nodiscard]] core::CommandChoice reference_choice(const core::FilePath &file,
                                                   const core::ProjectHitRow &row, bool opened)
{
    core::CommandChoice choice = core::project_hit_choice(file, row);
    if (opened)
    {
        return choice;
    }
    auto label = core::clipped_display_text(row.path + ":" + std::to_string(row.line.value));
    if (label.has_value())
    {
        choice.label = std::move(label).value();
    }
    return choice;
}

// 記号の候補（決定 5）。題名は「種類の印 名前」、補足は親の名前（無ければ無し）。表示できない名前の
// 記号は値なし。
[[nodiscard]] std::optional<core::CommandChoice> symbol_choice(const core::LspSymbol &symbol,
                                                               std::size_t index)
{
    auto label = core::clipped_display_text(std::string{core::lsp_symbol_kind_mark(symbol.kind)} +
                                            " " + symbol.name);
    if (!label.has_value())
    {
        return std::nullopt;
    }
    auto parent = core::clipped_display_text(symbol.parent);
    return core::CommandChoice{std::move(label).value(),
                               std::to_string(index),
                               core::CommandChoiceKind::land,
                               parent.has_value() ? std::optional{std::move(parent).value()}
                                                  : std::nullopt,
                               core::PaletteOrigin::symbol,
                               std::nullopt,
                               {}};
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
    held_references_subject_ = requested_subject();
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
// 閉じた・lsp をオフにした・席が止まった）に、文書を閉じる道の 1 か所から呼ぶ。記号も同じ。
void EditorController::forget_listed_answers(AutosaveId document)
{
    if (held_references_.has_value() && held_references_.value().context.document == document)
    {
        held_references_.reset();
        held_references_subject_.reset();
    }
    if (held_symbols_.has_value() && held_symbols_.value().second.context.document == document)
    {
        held_symbols_.reset();
    }
    // クイックフィックスの控え（#2005）も同じ。
    if (held_code_actions_.has_value() &&
        held_code_actions_.value().context.request.document == document)
    {
        held_code_actions_.reset();
    }
}

// `&` を見せるときに 1 回だけ作る参照の候補（同じ一覧の間は作り直さない・settle_listed_answers・
// #1958 のレビュー R1）。答えの順のまま。`file:` でない場所は
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
        ++listed_hits_made_;
        // 巨大な行かを 1 回だけ判定し、当たりと題名の両方に同じ値を渡す（巨大な行は開いていても
        // 行だけの当たりで「相対パス:行」・#1958 の確かめレビュー M1）。
        const bool exact =
            listed.second != nullptr && !giant_answer_line(*listed.second, location.range);
        const core::ProjectHit hit = exact ? opened_hit(*listed.second, location.range, encoding)
                                           : unopened_hit(location.range);
        core::CommandChoice choice =
            reference_choice(file, core::project_hit_row(root.value_or(file), file, hit), exact);
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

// 記号の答え（決定 5）も 1 回使って消す。使える条件・窓が非アクティブのとき・知らせは参照と同じ。
// 答えた文書を控え、一覧はその文書の記号だけ。Ctrl+P の一覧が `%` で開いている間に届いた答え
// （手で `%` を打って頼み直した答え・#1958）は、その一覧の候補にする。一覧が開いていて記号が `%`
// でない間（打ち直しの途中）に届いた答えも、照合（文書と改訂番号）は通っているので見せずに控える
// （`%` に戻れば held_symbols_current が通って出る・#1958 の確かめレビュー R1）。
void EditorController::settle_symbols_answer()
{
    auto answers = state_.language_answers();
    if (!answers.symbols.has_value())
    {
        return;
    }
    LanguageSymbols answered = std::move(answers.symbols).value();
    answers.symbols.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const bool listing = palette_scope_is(core::PaletteScope::symbols);
    const bool usable = listing || (faces_closed() && awaits_nothing());
    definition_anchor_.reset();
    if (!usable)
    {
        keep_symbols_answer(std::move(answered));
        return;
    }
    use_symbols_answer(std::move(answered), listing);
}

// 使える記号の答え（settle_symbols_answer の続き）。空なら知らせ、でなければ控えて見せる。
void EditorController::use_symbols_answer(LanguageSymbols answered, bool listing)
{
    const auto &path = state_.document().path;
    if (!path.has_value())
    {
        return;
    }
    if (answered.symbols.empty())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_symbols_not_found));
        return;
    }
    const std::size_t listed = answered.symbols.size();
    const std::size_t total = answered.total;
    held_symbols_ = std::pair{path.value(), std::move(answered)};
    if (!listing && window_inactive_)
    {
        return;
    }
    show_held_symbols(listing);
    if (total > listed)
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_symbols_first_of,
                                                  {std::to_string(listed), std::to_string(total)}));
    }
}

// 今は見せられない記号の答え。Ctrl+P の一覧が開いていれば（記号が `%` でない打ち直しの途中）、
// 照合は通っているので見せずに控える（空の答えは控えない・知らせない）。一覧が閉じていれば捨てる。
// 控えるだけで見せないので、窓が非アクティブかは見ない（非アクティブでも同じに控える）。
void EditorController::keep_symbols_answer(LanguageSymbols answered)
{
    const auto &input = state_.command_input();
    const bool palette =
        input.has_value() && std::holds_alternative<core::CommandPalette>(input.value());
    const auto &path = state_.document().path;
    if (!palette || !path.has_value() || answered.symbols.empty())
    {
        return;
    }
    held_symbols_ = std::pair{path.value(), std::move(answered)};
}

// 控えた記号を見せる。開いている `%` の一覧（listing）なら、前に見せていた記号の候補を捨ててから
// 作り直す（CommandPalette::dropped の 1 本）。でなければ `%` の一覧を開く。
void EditorController::show_held_symbols(bool listing)
{
    if (!listing)
    {
        run_palette_request(core::ExPaletteRequest{core::PaletteScope::symbols, {}});
        return;
    }
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    if (palette != nullptr)
    {
        state_ = state_.with_command_input(palette->dropped(core::PaletteOrigin::symbol));
    }
    symbols_listed_ = false;
    settle_listed_answers();
}

// 開いている Ctrl+P の一覧の出どころが scope か。
bool EditorController::palette_scope_is(core::PaletteScope scope) const
{
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    return palette != nullptr && palette->scope() == scope;
}

// 参照と記号の候補は、その記号（`&` / `%`）を見せるときにだけ作り、同じ一覧の間は 1 回だけ作って
// 持つ（#1958 のレビュー R1）。ほかの記号で開いた一覧・頼んでいない答えでは当たりを 1
// 件も作らない。 意図の出口（settle_transient）と一覧を開いた後に通る。`%`
// で控えた答えが今の文書の今の改訂の ものでなければ見せず、今の文書について頼み直す（gO
// と同じ道・#1958 の決め 4）。
void EditorController::settle_listed_answers()
{
    // クイックフィックス（`=`・#2005）は控えた答えを見せるだけで、頼み直さない。
    if (palette_scope_is(core::PaletteScope::code_actions) && !code_actions_listed_)
    {
        code_actions_listed_ = true;
        std::vector<core::CommandChoice> more;
        append_code_action_choices(more);
        extend_palette(more);
        return;
    }
    if (palette_scope_is(core::PaletteScope::references) && !references_listed_)
    {
        references_listed_ = true;
        std::vector<core::CommandChoice> more;
        append_reference_choices(more);
        extend_palette(more);
        return;
    }
    if (!palette_scope_is(core::PaletteScope::symbols) || symbols_listed_)
    {
        return;
    }
    if (held_symbols_current())
    {
        symbols_listed_ = true;
        std::vector<core::CommandChoice> more;
        append_symbol_choices(more);
        extend_palette(more);
        return;
    }
    if (!symbols_asked_)
    {
        symbols_asked_ = true;
        static_cast<void>(request_document_symbols());
    }
}

// 控えた記号の答えが今の文書の今の改訂のものか（改訂番号で比べる）。
bool EditorController::held_symbols_current() const
{
    if (!held_symbols_.has_value())
    {
        return false;
    }
    const LspRequestContext &context = held_symbols_.value().second.context;
    return context.document == state_.autosave_ids().at(state_.active_tab()) &&
           context.revision == static_cast<std::uint64_t>(state_.history().revision());
}

// 開いている一覧の候補の列の後ろへ足す（CommandPalette::extended の 1 本）。
void EditorController::extend_palette(const std::vector<core::CommandChoice> &more)
{
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    if (palette == nullptr || more.empty())
    {
        return;
    }
    state_ = state_.with_command_input(palette->extended(more));
}

// 一覧を開くたびに、参照と記号の候補を作り直す印を戻す（open_palette の 1 か所）。
void EditorController::reset_listed_answers()
{
    references_listed_ = false;
    symbols_listed_ = false;
    symbols_asked_ = false;
    code_actions_listed_ = false;
    palette_references_.clear();
    palette_symbols_.clear();
}

// 参照を頼んだ所（今の文書の root からの相対パス:行・#1958 の決め 3）。答えを使う時点の文脈は頼んだ
// ときと同じ（文書・revision・キャレット）なので、今の文書とキャレットから作る。
std::optional<core::DisplayText> EditorController::requested_subject() const
{
    const auto &path = state_.document().path;
    if (!path.has_value())
    {
        return std::nullopt;
    }
    const core::LineNumber line = state_.text().position_of(state_.selection().caret).line;
    const core::ProjectHit hit{line, core::Column{1}, {}, 0, 0, 0};
    const auto row =
        core::project_hit_row(project_search_root().value_or(path.value()), path.value(), hit);
    auto place = core::clipped_display_text(row.path + ":" + std::to_string(line.value));
    if (!place.has_value())
    {
        return std::nullopt;
    }
    return ui_notice(core::UiMessage::palette_references_subject, {place.value().text()});
}

// 参照の一覧（`&`）にだけ添える。
std::optional<core::DisplayText>
EditorController::held_references_subject(core::PaletteScope scope) const
{
    return scope == core::PaletteScope::references && held_references_.has_value()
               ? held_references_subject_
               : std::nullopt;
}

// 言語サーバーの出どころの候補が 0 のときの 1 行（#1958 の決め 2）。LSP がオフならそれを、参照と
// 記号は頼み方を（席が能力を持たなければ「答えません」）、診断は「診断はありません」。
// 鍵は操作の表から引く（文に鍵を直書きしない）。
std::optional<core::DisplayText>
EditorController::palette_empty_line(core::PaletteScope scope) const
{
    const auto empty = palette_listed_empty(scope);
    if (!empty.has_value() || !empty.value())
    {
        return std::nullopt;
    }
    if (!state_.settings().lsp)
    {
        return ui_notice(core::UiMessage::palette_empty_language_server_off);
    }
    const auto refusal = refusal_of(scope);
    if (!refusal.has_value())
    {
        return ui_notice(core::UiMessage::notice_no_diagnostics);
    }
    // 動いている席が能力を持たなければ、頼み方の代わりに鍵で頼んだときと同じ「答えません」
    // （#1958 の確かめレビュー L2）。
    if (lacks_ability(refusal.value().first))
    {
        return ui_notice(refusal.value().second);
    }
    if (scope == core::PaletteScope::code_actions)
    {
        return ui_notice(core::UiMessage::palette_empty_code_actions,
                         {chord_label_of(core::EditorOperation::quick_fix), vim_code_action_keys});
    }
    if (scope == core::PaletteScope::references)
    {
        return ui_notice(
            core::UiMessage::palette_empty_references,
            {chord_label_of(core::EditorOperation::list_references), vim_references_keys});
    }
    const std::string_view name = core::ui_text(
        core::operation_text(core::EditorOperation::list_symbols).name, state_.display_language());
    return ui_notice(
        core::UiMessage::palette_empty_symbols,
        {vim_symbols_keys, chord_label_of(core::EditorOperation::list_operations), name});
}

// 言語サーバーの出どころの一覧が空か（候補を作ったときの列で数える・クイックフィックスは控えを
// 出せるか）。ほかの出どころは値なし。
std::optional<bool> EditorController::palette_listed_empty(core::PaletteScope scope) const
{
    switch (scope)
    {
    case core::PaletteScope::references:
        return palette_references_.empty();
    case core::PaletteScope::symbols:
        return palette_symbols_.empty();
    case core::PaletteScope::diagnostics:
        return palette_diagnostics_.empty();
    case core::PaletteScope::code_actions:
        return !code_actions_shown();
    case core::PaletteScope::files:
    case core::PaletteScope::tabs:
    case core::PaletteScope::bookmarks:
    case core::PaletteScope::history:
    case core::PaletteScope::folder:
    case core::PaletteScope::commands:
    case core::PaletteScope::operations:
    case core::PaletteScope::project_hits:
        return std::nullopt;
    }
    std::unreachable();
}

std::size_t EditorController::listed_hits_made() const noexcept
{
    return listed_hits_made_;
}

// 見せるときに 1 回だけ作る記号の候補（文書の順）。文書が開いていれば本文で行と印を写す。
void EditorController::append_symbol_choices(std::vector<core::CommandChoice> &entries)
{
    palette_symbols_.clear();
    if (!held_symbols_.has_value())
    {
        return;
    }
    const auto &[file, answer] = held_symbols_.value();
    const auto tab = open_tab_of(file);
    const auto text = tab.has_value() ? tab_text_at(tab.value()) : std::nullopt;
    const core::TextBuffer *body = text.has_value() ? text.value().first : nullptr;
    for (const core::LspSymbol &symbol : answer.symbols)
    {
        auto choice = symbol_choice(symbol, palette_symbols_.size());
        if (!choice.has_value())
        {
            continue;
        }
        ++listed_hits_made_;
        const core::ProjectHit hit = listed_hit(symbol.at, body, answer.encoding);
        entries.push_back(std::move(choice).value());
        palette_symbols_.push_back(
            PaletteReference{file, LspLanding{.at = symbol.at.start, .encoding = answer.encoding},
                             PreviewAnchor{hit.line, hit.match_begin, hit.match_end}});
    }
}

// 参照と記号の候補の指す所。出どころで控えた列を選ぶ。
std::optional<PaletteReference>
EditorController::palette_reference_of(const core::CommandChoice &choice) const
{
    if (choice.kind != core::CommandChoiceKind::land || !choice.origin.has_value())
    {
        return std::nullopt;
    }
    const std::vector<PaletteReference> *listed = listed_references_of(choice.origin.value());
    const auto index = listed_index(choice.command);
    if (listed == nullptr || !index.has_value() || index.value() >= listed->size())
    {
        return std::nullopt;
    }
    return listed->at(index.value());
}

// 出どころ → 控えた指す所の列（参照と記号・ほかは無し）。閉じた switch（#1958 のレビュー L4）。
const std::vector<PaletteReference> *
EditorController::listed_references_of(core::PaletteOrigin origin) const noexcept
{
    switch (origin)
    {
    case core::PaletteOrigin::reference:
        return &palette_references_;
    case core::PaletteOrigin::symbol:
        return &palette_symbols_;
    case core::PaletteOrigin::code_action:
    case core::PaletteOrigin::tab:
    case core::PaletteOrigin::bookmarked_tab:
    case core::PaletteOrigin::bookmark:
    case core::PaletteOrigin::history:
    case core::PaletteOrigin::folder:
    case core::PaletteOrigin::document_format:
    case core::PaletteOrigin::project_hit:
    case core::PaletteOrigin::diagnostic:
        return nullptr;
    }
    std::unreachable();
}

// 跳ぶ候補を選ぶ（CommandChoiceKind::land）。参照と記号はここ、診断（#1939）は診断の道。出どころは
// 閉じた switch で分ける（#1958 のレビュー L4）。跳ばない出どころは何もしない。
void EditorController::land_palette_choice(const core::CommandChoice &choice)
{
    if (!choice.origin.has_value())
    {
        return;
    }
    switch (choice.origin.value())
    {
    case core::PaletteOrigin::reference:
    case core::PaletteOrigin::symbol:
        land_palette_reference(choice);
        return;
    case core::PaletteOrigin::diagnostic:
        land_palette_diagnostic(choice);
        return;
    // クイックフィックス（#2005）は跳ばずに当てる。
    case core::PaletteOrigin::code_action:
        choose_code_action(choice);
        return;
    case core::PaletteOrigin::tab:
    case core::PaletteOrigin::bookmarked_tab:
    case core::PaletteOrigin::bookmark:
    case core::PaletteOrigin::history:
    case core::PaletteOrigin::folder:
    case core::PaletteOrigin::document_format:
    case core::PaletteOrigin::project_hit:
        return;
    }
}

// 候補を選ぶ（決定 4・5）。面を閉じ、定義と同じ着地の 1 本（開いていなければ開いてから・`''`
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
