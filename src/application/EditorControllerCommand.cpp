#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CommandEdit.hpp"
#include "CommandInput.hpp"
#include "CommandLayout.hpp"
#include "CommandPalette.hpp"
#include "DisplayText.hpp"
#include "EditBoundary.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExLineRequest.hpp"
#include "ExMapCommand.hpp"
#include "ExMapRequest.hpp"
#include "ExMapVerb.hpp"
#include "ExResult.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "FindPhase.hpp"
#include "FindState.hpp"
#include "FontFamilies.hpp"
#include "GoToLineInput.hpp"
#include "InputLinePrompt.hpp"
#include "LiteralPattern.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PaletteLayout.hpp"
#include "PaletteMarks.hpp"
#include "SearchLine.hpp"
#include "StatusBarLayout.hpp"
#include "SubstituteAll.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimPattern.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimStep.hpp"
#include "VimStepSource.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::application
{
using detail::CommandWindow;
using detail::measured;

namespace
{
// 入力行の編集のうち履歴を歩く 2 つ（#725）。写し先が足りなければコンパイルが落ちる（CPP-002）。
[[nodiscard]] constexpr bool recalls_history(core::CommandEdit edit) noexcept
{
    switch (edit)
    {
    case core::CommandEdit::history_previous:
    case core::CommandEdit::history_next:
        return true;
    case core::CommandEdit::left:
    case core::CommandEdit::right:
    case core::CommandEdit::home:
    case core::CommandEdit::end:
    case core::CommandEdit::backspace:
    case core::CommandEdit::erase:
    case core::CommandEdit::complete_next:
    case core::CommandEdit::complete_previous:
    case core::CommandEdit::page_previous:
    case core::CommandEdit::page_next:
    case core::CommandEdit::list_first:
    case core::CommandEdit::list_last:
        return false;
    }
    std::unreachable();
}

// 行数が変わるときだけ替えた入力（同じなら無し・写しを作らない）。
[[nodiscard]] std::optional<CommandInput> fitted_rows(const core::CommandPalette &palette,
                                                      CommandWindow window)
{
    const auto [width, height, dpi] = window;
    const auto rows =
        measured(window) ? core::palette_window_rows(width, height, dpi) : core::palette_row_limit;
    if (std::max(rows, std::size_t{1}) == palette.visible())
    {
        return std::nullopt;
    }
    return CommandInput{palette.with_visible(rows)};
}

[[nodiscard]] std::optional<CommandInput> fitted_rows(const core::CommandLine &line,
                                                      CommandWindow window)
{
    const auto [width, height, dpi] = window;
    const auto rows = measured(window)
                          ? core::command_layout(core::status_bar_layout(width, height, dpi), dpi,
                                                 core::command_completion_row_limit)
                                .visible_rows
                          : core::command_completion_row_limit;
    if (std::max(rows, std::size_t{1}) == line.completion_rows())
    {
        return std::nullopt;
    }
    return CommandInput{line.with_completion_rows(rows)};
}

[[nodiscard]] std::optional<CommandInput> fitted_rows(const core::SearchLine & /*line*/,
                                                      CommandWindow /*window*/)
{
    return std::nullopt;
}

// Vim の `/` `?` の入力行か（engine の検索の待ちを持つ）。写し先が足りなければ落ちる（CPP-002）。
[[nodiscard]] constexpr bool vim_search_prompt(core::InputLinePrompt prompt) noexcept
{
    switch (prompt)
    {
    case core::InputLinePrompt::search_forward:
    case core::InputLinePrompt::search_backward:
        return true;
    case core::InputLinePrompt::find:
    case core::InputLinePrompt::replace_with:
    case core::InputLinePrompt::go_to_line:
    case core::InputLinePrompt::ex:
    case core::InputLinePrompt::palette:
        return false;
    }
    std::unreachable();
}

// 「行: 」が数でないときの知らせ（ADR 0174 の決定 8）。
constexpr std::string_view not_a_line_notice = "行番号を入れてください";
// Vim モードで一覧から「置換」を選んだときの `:` の前置き（ADR 0174 の決定 2）。
constexpr std::string_view vim_replace_prefix = "%s/";
// 置換の起点を戻す幅（UTF-8 の 1 コードポイントの最大のバイト数）。
constexpr std::size_t code_point_bytes = 4;
} // namespace

bool EditorController::command_line_active() const noexcept
{
    return state_.command_input().has_value();
}

bool EditorController::find_line_active() const noexcept
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return false;
    }
    const auto *line = std::get_if<core::SearchLine>(&input.value());
    return line != nullptr && (line->prompt() == core::InputLinePrompt::find ||
                               line->prompt() == core::InputLinePrompt::replace_with);
}

std::optional<core::InputLineView> EditorController::command_line_view() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return std::nullopt;
    }
    auto view = input_line_of(input.value());
    // 「置換後: 」は 2 つの欄を同時に見せられないので、検索語をプロンプトに添える（ADR 0174
    // の結果）。
    if (view.prompt == core::InputLinePrompt::replace_with)
    {
        view.prompt_detail = state_.find().pattern;
    }
    return view;
}

void EditorController::accept(const CommandText &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    auto next = inserted_command(input.value(), intent.utf8);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    state_ = state_.with_command_input(std::move(next).value());
    reach_font_families();
    update_search_preview();
}

void EditorController::accept(const EditCommand &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    // 空の入力行の Backspace は取消（Ex と検索で同じ・ADR 0032 の決定 1）。
    // 設定一覧は候補を出したまま残る。
    const bool palette = std::holds_alternative<core::CommandPalette>(input.value());
    if (!palette && input_line_of(input.value()).text.empty() &&
        intent.edit == core::CommandEdit::backspace)
    {
        accept(CancelCommand{});
        return;
    }
    state_ = state_.with_command_input(recalls_history(intent.edit)
                                           ? recalled_command(input.value(), state_.ex_history(),
                                                              state_.search_history(), intent.edit)
                                           : edited_command(input.value(), intent.edit));
    reach_font_families();
    update_search_preview();
}

void EditorController::fit_command_rows()
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    const CommandWindow window{window_width_, window_height_, view_geometry_.dpi};
    auto fitted = std::visit([window](const auto &value) { return fitted_rows(value, window); },
                             input.value());
    if (fitted.has_value())
    {
        state_ = state_.with_command_input(std::move(fitted).value());
    }
}

void EditorController::accept(const CancelCommand &)
{
    const auto &input = state_.command_input();
    // Vim の `/` `?` の取消だけ。通常モードの「検索: 」「行: 」は engine の待ちを持たない。
    const auto *line = input.has_value() ? std::get_if<core::SearchLine>(&input.value()) : nullptr;
    const bool searching = line != nullptr && vim_search_prompt(line->prompt());
    // 入力行を閉じると preview も消えるので、戻す先を先に写す（ADR 0041 の決定 5）。
    const auto preview = state_.search_preview();
    close_command_input();
    if (preview.has_value())
    {
        restore_search_origin(preview.value().origin);
    }
    if (searching)
    {
        // 検索の取消は保留中のオペレータと回数も捨てる（`d/<Esc>` のあとの `x` が消すのと
        // 同じ・実測）。何を捨てるかは engine の純関数が決める（ADR 0032 の決定 1）。
        state_ = state_.with_vim(core::vim_cancelled_input(state_.vim()));
    }
}

void EditorController::accept(const PasteCommand &)
{
    if (!state_.command_input().has_value())
    {
        return;
    }
    const auto text = ports_.clipboard.read();
    if (!text)
    {
        state_ =
            state_.with_command_message(core::DisplayText::parse("Clipboard unavailable").value());
        return;
    }
    accept(CommandText{text.value()});
}

std::optional<core::VimRepeatFailure> EditorController::submit(const core::CommandLine &line)
{
    evaluate_command(std::string(line.text()));
    return std::nullopt;
}

std::optional<core::VimRepeatFailure> EditorController::submit(const core::CommandPalette &palette)
{
    submit_palette(palette);
    return std::nullopt;
}

// 検索の確定は engine の 1 つの鍵（ADR 0032 の決定 3）。先に入力行を閉じてから送る。
// 通常モードの「検索: 」は語を逃がした鍵で入力行を開いたまま、「行: 」は `:N` と
// 同じ着地（ADR 0174 の決定 3・8）。
std::optional<core::VimRepeatFailure> EditorController::submit(const core::SearchLine &line)
{
    switch (line.prompt())
    {
    case core::InputLinePrompt::find:
        switch (state_.find().phase)
        {
        case FindPhase::find:
            return hop_find(line.text(), core::VimSearchDirection::forward, true);
        case FindPhase::replace_with:
            return advance_to_replacement(line);
        }
        std::unreachable();
    case core::InputLinePrompt::replace_with:
        return replace_current(line);
    case core::InputLinePrompt::go_to_line:
        go_to_line(line);
        return std::nullopt;
    case core::InputLinePrompt::search_forward:
    case core::InputLinePrompt::search_backward:
    case core::InputLinePrompt::ex:
    case core::InputLinePrompt::palette:
        break;
    }
    // Vim も確定の前に入力前の画面へ戻してから本当の検索をする（ex_getln.c の
    // finish_incsearch_highlighting）。着いた先は engine の鍵のあとの follow_caret が見せる。
    // 鍵は preview の起点を運ぶ（Ctrl-G / Ctrl-T が無ければキャレットと同じ・ADR 0043 の決定 3）。
    const auto preview = state_.search_preview();
    const std::optional<core::TextPosition> from =
        preview.has_value() ? std::optional{preview.value().from} : std::nullopt;
    const core::VimSearchPattern pattern{std::string(line.text()), line.direction(), from};
    close_command_input();
    if (preview.has_value())
    {
        restore_search_origin(preview.value().origin);
    }
    const auto failure = step_vim(core::VimKey{pattern});
    // 検索の失敗（E486 E35 未対応の構文）は engine が not_found で返し、入力行の確定なので鳴らす。
    // 同じ報せでも後の `n` は鍵なので鳴らさない。layout の契約の破れ（refused）は使う人の断りでは
    // ないので鳴らさない（ADR 0153 の決定 1・2）。
    const auto &message = state_.command_message();
    if (failure == core::VimRepeatFailure::not_found && message.has_value())
    {
        reject(message.value());
    }
    return failure;
}

void EditorController::accept(const SubmitCommand &intent)
{
    // Ctrl+Enter は「置換後: 」の全件だけ（ADR 0174 の決定 3・7）。
    if (intent.all)
    {
        replace_all_matches();
        return;
    }
    static_cast<void>(confirmed_command());
}

// 確定の意図（Enter）だけが入力を履歴に足す。Esc は取消で、再生の中の <Esc> の確定
// （submitted_command を直接呼ぶ）も足さない（#725・ADR 0022 決定 2 の追補）。
std::optional<core::VimRepeatFailure> EditorController::confirmed_command()
{
    const auto &open = state_.command_input();
    if (open.has_value())
    {
        const CommandInput input = open.value();
        std::visit([this](const auto &value) { this->remember(value); }, input);
    }
    return submitted_command();
}

void EditorController::remember(const core::CommandLine &line)
{
    state_ = state_.with_ex_history(state_.ex_history().remembered(line.text()));
}

// 「検索: 」の語も検索の履歴の 1 本へ（入力行の履歴は共用・ADR 0174 の決定 3）。
// 「行: 」と「置換後: 」は足さない。
void EditorController::remember(const core::SearchLine &line)
{
    if (line.prompt() == core::InputLinePrompt::go_to_line ||
        line.prompt() == core::InputLinePrompt::replace_with)
    {
        return;
    }
    state_ = state_.with_search_history(state_.search_history().remembered(line.text()));
}

void EditorController::remember(const core::CommandPalette & /*palette*/)
{
    // 面の確定は履歴に足さない（#725）。
}

// 確定の失敗（検索の当たりが無い等）は再生を打ち切るので理由を返す（ADR 0048 の決定 8）。
std::optional<core::VimRepeatFailure> EditorController::submitted_command()
{
    const auto &open = state_.command_input();
    if (!open.has_value())
    {
        return std::nullopt;
    }
    // 入力行は state_ が持つので、写し先が状態を書き換える前に値ごと複製する。
    const CommandInput input = open.value();
    return std::visit([this](const auto &value) { return this->submit(value); }, input);
}

// 鳴らすかは呼んだ道で決め、文字列（E で始まるか）では決めない（ADR 0153 の決定 3）。
void EditorController::reject(core::DisplayText message)
{
    state_ = state_.with_command_message(std::move(message));
    ports_.bell.ring();
}

void EditorController::evaluate_command(std::string_view text)
{
    close_command_input();
    if (text.empty())
    {
        return;
    }
    const auto result =
        core::evaluate_ex(text, state_.settings(), state_.appearance(), state_.themes());
    if (!result)
    {
        reject(core::ex_failure_message(result.error(), text));
        return;
    }
    if (ran_ex_request(result.value()))
    {
        return;
    }
    const auto &settings = result.value().settings;
    if (settings.has_value() && !persist_settings(settings.value()))
    {
        reject(core::DisplayText::parse("Settings could not be saved").value());
        return;
    }
    apply_unsaved_ex(result.value());
    state_ = state_.with_command_message(result.value().message);
}

// Ex の答えのうち命令の要求（タブ・行番号・窓・面・文書・ルート・言語・構文・クリップボード）を
// 1 つ実行したら真。ルート（`:set root=.`）だけは解いた値を設定へ書き、ほかは設定を保存しない。
// 知らせは写し先が決める。
bool EditorController::ran_ex_request(const core::ExResult &result)
{
    if (result.tab.has_value())
    {
        run_tab_request(result.tab.value());
        return true;
    }
    if (result.line.has_value())
    {
        jump_to_line(result.line.value());
        return true;
    }
    if (result.window.has_value())
    {
        run_window_command(result.window.value());
        return true;
    }
    if (result.palette.has_value())
    {
        run_palette_request(result.palette.value());
        return true;
    }
    if (result.document.has_value())
    {
        run_document_request(result.document.value());
        return true;
    }
    // `:set root=.` は文書のフォルダを解いて設定へ書く（ADR 0162 の決定 7）。
    if (result.root.has_value())
    {
        run_root_request(result.root.value());
        return true;
    }
    if (result.map.has_value())
    {
        run_map_request(result.map.value());
        return true;
    }
    return answer_current_value(result);
}

// Ex の答えのうち保存しない値（表示形式・検索の強調・incsearch）を写す。
void EditorController::apply_unsaved_ex(const core::ExResult &result)
{
    // 表示形式は文書の値で、窓の問いの「はい」と同じ 1 本を通る（ADR 0140 の決定 3）。
    const auto &display = result.display;
    if (display.has_value())
    {
        accept(SelectDisplay{display.value()});
    }
    // 検索の当たりの強調は設定ではなく Vim の状態なので、保存せずここで写す（ADR 0037 の決定 2）。
    const auto &highlight = result.highlight;
    if (highlight.has_value())
    {
        core::VimState vim = state_.vim();
        vim.highlight = core::requested_highlight(vim.highlight, highlight.value());
        state_ = state_.with_vim(std::move(vim));
    }
    // incsearch も同じく Vim の状態で、保存しない（ADR 0041 の決定 6）。
    const auto &incsearch = result.incsearch;
    if (incsearch.has_value())
    {
        core::VimState vim = state_.vim();
        vim.incsearch = incsearch.value();
        state_ = state_.with_vim(std::move(vim));
    }
}

void EditorController::run_palette_request(const core::ExPaletteRequest &request)
{
    const bool active = request.scope == core::PaletteScope::tabs && request.query.empty();
    open_palette(core::palette_input_for(core::PaletteQuery{request.scope, request.query}),
                 active ? state_.active_tab() : 0);
}

// `map` の要求は設定の行を書き換えて v4 へ保存し、表は persist_settings が作り直す（ADR 0176 の
// 決定 6）。成功は Vim と同じく何も出さない。引数なしの一覧（Ctrl+P の読むだけの出どころ）は
// まだ無い（出どころの記号が決まっていない）ので断る。
void EditorController::run_map_request(const core::ExMapRequest &request)
{
    switch (request.verb)
    {
    case core::ExMapVerb::list:
        reject(core::DisplayText::parse("Not supported: listing mappings").value());
        return;
    case core::ExMapVerb::add:
    case core::ExMapVerb::remove:
    case core::ExMapVerb::leader:
        break;
    }
    const auto settings = core::map_edited(state_.settings(), request);
    if (!settings)
    {
        reject(core::ex_map_failure_message(settings.error()));
        return;
    }
    if (!persist_settings(settings.value()))
    {
        reject(core::DisplayText::parse("Settings could not be saved").value());
    }
}

// `:N` は鍵の `{N}G` と同じ engine の着地を同じ写しの道で写す（2 本にしない）。着地は必ず本文の中で
// 失敗しないので、断りの知らせも警告音も無い（Vim も `:8000` は黙って最後の行へ）。
void EditorController::jump_to_line(const core::ExLineRequest &request)
{
    static_cast<void>(stepped_vim(VimStepSource{request}));
}

// 行き先は gt / gT と同じ tab_destination の 1 本。範囲の外の数は Vim の実測の文言（決定 4・5）。
// `:tabclose` は状態を変えず、閉じたいタブの位置だけを載せる（決定 6）。
void EditorController::run_tab_request(const core::ExTabRequest &request)
{
    switch (request.verb)
    {
    case core::ExTabVerb::next:
    case core::ExTabVerb::previous:
        break;
    case core::ExTabVerb::open:
        accept(NewTab{});
        return;
    case core::ExTabVerb::close:
        state_ = state_.with_close_request(state_.active_tab());
        return;
    case core::ExTabVerb::list:
        accept(OpenTabList{});
        return;
    }
    const auto direction = request.verb == core::ExTabVerb::next ? core::TabJumpDirection::forward
                                                                 : core::TabJumpDirection::backward;
    const auto destination = core::tab_destination(core::TabJump{direction, request.number},
                                                   state_.active_tab(), state_.tab_count());
    if (!destination.has_value())
    {
        reject(core::DisplayText::parse("E475: Invalid argument: " +
                                        std::to_string(request.number.value_or(0)))
                   .value());
        return;
    }
    accept(SwitchTab{destination.value()});
}

// 読んだ字体の一覧（まだ読んでいなければ空の列）。
core::FontFamilies EditorController::given_fonts() const
{
    return font_families_.value_or(core::FontFamilies::none());
}

// 入力行が字体の補完を求めたら、字体の一覧をプロセスで 1 回だけ読んで入力行へ入れる（#597・D58）。
// 一覧は変わらないものとして再読しない。読めなければ空の列のまま（候補は打った値だけ・知らせなし）。
void EditorController::reach_font_families()
{
    const auto &input = state_.command_input();
    if (font_families_.has_value() || !input.has_value() || !wants_font_families(input.value()))
    {
        return;
    }
    auto listed = ports_.fonts.list();
    font_families_ = listed.has_value() ? core::FontFamilies::from(std::move(listed).value())
                                        : core::FontFamilies::none();
    state_ = state_.with_command_input(with_font_families(input.value(), font_families_.value()));
}

// ------------------------------------------------------- 通常モードの検索と行へ移動（ADR 0174）

void EditorController::accept(const BeginFind &)
{
    switch (state_.mode())
    {
    case core::EditMode::vim:
        static_cast<void>(step_vim_command(core::VimKey{core::VimCharacter{U'/'}}));
        return;
    case core::EditMode::ordinary:
        break;
    }
    open_find(FindPhase::find);
}

void EditorController::open_find(FindPhase phase)
{
    FindState find = state_.find();
    find.phase = phase;
    state_ = state_.with_find(std::move(find));
    // Vim の `/` と同じく、開いたときのスクロールとキャレットを preview の戻り先と起点にする
    // （ADR 0041 の決定 3）。前回の語を置くので、開いた時点で当たりを見せる。
    const ScrollState origin = settled_scroll();
    state_ = state_.with_command_input(core::SearchLine::opened_find(state_.find().pattern));
    state_ = state_.with_search_preview(
        SearchPreview{std::nullopt, origin, state_.text().position_of(state_.selection().caret)});
    update_search_preview();
}

void EditorController::accept(const BeginGoToLine &)
{
    switch (state_.mode())
    {
    case core::EditMode::vim:
        static_cast<void>(step_vim_command(core::VimKey{core::VimCharacter{U':'}}));
        return;
    case core::EditMode::ordinary:
        break;
    }
    state_ = state_.with_command_input(core::SearchLine::opened_go_to_line());
}

// Vim モードは `:` の入力行に `%s/` を前置きする（決定 2・確定は ADR 0168 の `:s`）。通常モードは
// 置換の流れの「検索: 」（決定 3）。
void EditorController::accept(const BeginReplace &)
{
    switch (state_.mode())
    {
    case core::EditMode::vim:
        static_cast<void>(step_vim_command(core::VimKey{core::VimCharacter{U':'}}));
        accept(CommandText{std::string(vim_replace_prefix)});
        return;
    case core::EditMode::ordinary:
        break;
    }
    open_find(FindPhase::replace_with);
}

// 「今の当たり」（キャレットを含むか次の当たり・決定
// 7）へ移る。起点をその当たりの直前の文字に置いて hop_find
// で探すので、キャレットの始まりにある当たりも飛ばさず、最後の検索と塗りは「検索: 」の Enter
// と同じになる。当たりが本文の先頭なら起点を置けないので、探した後にキャレットを戻す。
std::optional<core::VimRepeatFailure>
EditorController::advance_to_replacement(const core::SearchLine &line)
{
    if (line.text().empty())
    {
        return std::nullopt;
    }
    FindState find = state_.find();
    find.pattern = std::string(line.text());
    state_ = state_.with_find(std::move(find));
    const auto pattern = find_pattern();
    const auto match = pattern.has_value() ? current_find_match(state_.text(), pattern.value(),
                                                                state_.selection().caret)
                                           : std::nullopt;
    if (match.has_value())
    {
        restart_find_before(match.value().begin);
    }
    const auto failure = hop_find(line.text(), core::VimSearchDirection::forward, true);
    if (failure.has_value())
    {
        return failure;
    }
    if (match.has_value() && state_.selection().caret != match.value().begin)
    {
        move_caret_to(match.value().begin, core::SelectionAnchoring::collapse);
    }
    state_ =
        state_.with_command_input(core::SearchLine::opened_replace_with(state_.find().replacement));
    return std::nullopt;
}

std::optional<core::VimPattern> EditorController::find_pattern() const
{
    const std::string &typed = state_.find().pattern;
    if (typed.empty())
    {
        return std::nullopt;
    }
    auto parsed =
        core::VimPattern::parse(core::literal_pattern(typed), core::VimSearchDirection::forward,
                                core::search_case(state_.settings()));
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

// 今の当たり（キャレットを含むか次の当たり・current_find_match の 1 本）を置き換える Edit 1 回
// （undo 1 単位・restore はそのときのキャレット）。当たりが無ければ「検索: 」の Enter と同じ E486。
// 置き換えた後は次の当たりへ移る（最後の 1 件の後の E486 は確定の断りではないので鳴らさない）。
std::optional<core::VimRepeatFailure>
EditorController::replace_current(const core::SearchLine &line)
{
    FindState find = state_.find();
    find.replacement = std::string(line.text());
    state_ = state_.with_find(find);
    const auto pattern = find_pattern();
    if (!pattern.has_value())
    {
        return std::nullopt;
    }
    const auto match = current_find_match(state_.text(), pattern.value(), state_.selection().caret);
    if (!match.has_value())
    {
        return hop_find(find.pattern, core::VimSearchDirection::forward, true);
    }
    replace(match.value(), find.replacement, core::EditBoundary::separate);
    restart_find_before(state_.selection().caret);
    return hop_find(find.pattern, core::VimSearchDirection::forward, false);
}

void EditorController::replace_all_matches()
{
    const auto &input = state_.command_input();
    const auto *line = input.has_value() ? std::get_if<core::SearchLine>(&input.value()) : nullptr;
    if (line == nullptr || line->prompt() != core::InputLinePrompt::replace_with)
    {
        return;
    }
    FindState find = state_.find();
    find.replacement = std::string(line->text());
    state_ = state_.with_find(find);
    const auto pattern = find_pattern();
    if (!pattern.has_value())
    {
        return;
    }
    const auto edit = core::substituted_all(state_.text(), pattern.value(), find.replacement);
    if (!edit.has_value())
    {
        static_cast<void>(hop_find(find.pattern, core::VimSearchDirection::forward, true));
        return;
    }
    const core::Offset end{edit.value().at.value + edit.value().removed.size()};
    replace(core::OffsetRange{edit.value().at, end}, edit.value().inserted,
            core::EditBoundary::separate);
    restart_find_before(state_.selection().caret);
}

// 起点（preview の from）は「その次の文字から探す」位置なので、at の直前の文字に置く。
// at が本文の先頭なら先頭（先頭の当たりは折り返して最後に着く・限界）。
void EditorController::restart_find_before(core::Offset at)
{
    core::Offset before = at;
    if (at.value > 0)
    {
        const std::size_t window = std::min<std::size_t>(at.value, code_point_bytes);
        const core::Offset begin{at.value - window};
        const std::string tail = state_.text().text_range(begin, at);
        before = core::Offset{begin.value +
                              core::previous_code_point(tail, core::Offset{tail.size()}).value};
    }
    state_ = state_.with_search_preview(
        SearchPreview{std::nullopt, settled_scroll(), state_.text().position_of(before)});
}

std::optional<core::VimRepeatFailure> EditorController::step_vim_command(core::VimKey key)
{
    if (state_.mode() == core::EditMode::vim && state_.vim().mode == core::VimMode::insert)
    {
        static_cast<void>(step_vim(core::VimKey{core::VimSpecialKey::escape}));
    }
    return step_vim(key);
}

// 語は Vim の最後の検索と同じ値（逃がした形）として engine の鍵 1 つで探す（決定 4・5・6）。起点は
// preview の起点（開いたときか前の当たりのキャレット）で、着いた後は入力行を開いたまま、今の画面と
// キャレットを次の起点にする。空の語は何もしない。
std::optional<core::VimRepeatFailure>
EditorController::hop_find(std::string_view word, core::VimSearchDirection relative, bool confirmed)
{
    const std::string typed(word);
    if (typed.empty())
    {
        return std::nullopt;
    }
    FindState find = state_.find();
    find.pattern = typed;
    state_ = state_.with_find(std::move(find));
    const auto preview = state_.search_preview();
    const std::optional<core::TextPosition> from =
        preview.has_value() ? std::optional{preview.value().from} : std::nullopt;
    if (preview.has_value())
    {
        restore_search_origin(preview.value().origin);
    }
    const auto failure = step_vim(
        core::VimKey{core::VimSearchPattern{core::literal_pattern(typed), relative, from}});
    // 鳴らすのは Enter の断りだけ（F3 は鍵・ADR 0153 の決定 1・2）。
    const auto &message = state_.command_message();
    if (confirmed && failure == core::VimRepeatFailure::not_found && message.has_value())
    {
        reject(message.value());
    }
    state_ = state_.with_search_preview(SearchPreview{
        std::nullopt, settled_scroll(), state_.text().position_of(state_.selection().caret)});
    return failure;
}

void EditorController::repeat_find(core::VimSearchDirection relative)
{
    const auto &last = state_.vim().last_search;
    const bool along = !last.has_value() || last.value().direction() == relative;
    static_cast<void>(step_vim_command(core::VimKey{core::VimCharacter{along ? U'n' : U'N'}}));
}

void EditorController::go_to_line(const core::SearchLine &line)
{
    const std::string typed(line.text());
    close_command_input();
    if (typed.empty())
    {
        return;
    }
    const auto target = core::go_to_line_target(typed);
    if (!target.has_value())
    {
        reject(core::DisplayText::parse(not_a_line_notice).value());
        return;
    }
    jump_to_line(core::ExLineRequest{target.value().line});
    const auto column = target.value().column;
    if (!column.has_value())
    {
        return;
    }
    const core::LineNumber landed = state_.text().position_of(state_.selection().caret).line;
    accept(PlaceCaret{
        core::TextPosition{landed, core::Column{std::max<std::size_t>(column.value(), 1)}},
        core::SelectionAnchoring::collapse});
}

void EditorController::close_command_input()
{
    // 入力行の変換は入力行と一緒に消える（ADR 0061 の決定 3・#635 の追記）。
    const bool opened = command_line_active();
    state_ = state_.with_command_input(std::nullopt);
    if (opened)
    {
        state_ = state_.with_composition(std::nullopt);
    }
}
} // namespace nenenib::application
