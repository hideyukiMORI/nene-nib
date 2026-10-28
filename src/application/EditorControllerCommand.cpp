#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CommandEdit.hpp"
#include "CommandInput.hpp"
#include "CommandLayout.hpp"
#include "CommandPalette.hpp"
#include "DisplayText.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExLineRequest.hpp"
#include "ExResult.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "FontFamilies.hpp"
#include "PaletteLayout.hpp"
#include "PaletteMarks.hpp"
#include "SearchLine.hpp"
#include "StatusBarLayout.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "TextPosition.hpp"
#include "VimKey.hpp"
#include "VimSearchPattern.hpp"
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
} // namespace

bool EditorController::command_line_active() const noexcept
{
    return state_.command_input().has_value();
}

std::optional<core::InputLineView> EditorController::command_line_view() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return std::nullopt;
    }
    return input_line_of(input.value());
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
    const bool searching =
        input.has_value() && std::holds_alternative<core::SearchLine>(input.value());
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
std::optional<core::VimRepeatFailure> EditorController::submit(const core::SearchLine &line)
{
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

void EditorController::accept(const SubmitCommand &)
{
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

void EditorController::remember(const core::SearchLine &line)
{
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

// Ex の答えのうち命令の要求（タブ・行番号・窓・面・文書・言語・構文・クリップボード）を 1 つ実行
// したら真。どれも設定を保存せず、知らせは写し先が決める。
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
