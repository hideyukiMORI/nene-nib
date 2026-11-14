#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CommandEdit.hpp"
#include "CommandInput.hpp"
#include "CommandLayout.hpp"
#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "DisplayLanguageNames.hpp"
#include "DisplayText.hpp"
#include "DocumentByteLimit.hpp"
#include "EditBoundary.hpp"
#include "EditMode.hpp"
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
#include "QuantityText.hpp"
#include "ResolvedLanguage.hpp"
#include "SearchLine.hpp"
#include "StatusBarLayout.hpp"
#include "SubstituteAll.hpp"
#include "SubstituteFailure.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimCommandMotion.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimMode.hpp"
#include "VimPattern.hpp"
#include "VimRecordedKeys.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimStep.hpp"
#include "VimStepSource.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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

// Vim の入力行（`:` `/` `?`）の Delete は Vim の
// c_<Del>（cmdline_erase_chars・#1325）。キャレットの 下の文字を消し、行末では手前の 1
// 文字を消す（Backspace と同じ・空の行では取消になる）。 ほかの入力行（通常モードの「検索: 」「行:
// 」と面）の Delete は今のまま（ADR 0065 の決定 6）。
[[nodiscard]] core::CommandEdit vim_line_edit(const core::InputLineView &line,
                                              core::CommandEdit edit) noexcept
{
    const bool vim_line =
        line.prompt == core::InputLinePrompt::ex || vim_search_prompt(line.prompt);
    if (edit == core::CommandEdit::erase && vim_line && line.caret.value >= line.text.size())
    {
        return core::CommandEdit::backspace;
    }
    return edit;
}

// Vim モードで一覧から「置換」を選んだときの `:` の前置き（ADR 0174 の決定 2）。
constexpr std::string_view vim_replace_prefix = "%s/";
// 置換の起点を戻す幅（UTF-8 の 1 コードポイントの最大のバイト数）。
constexpr std::size_t code_point_bytes = 4;

// 入力行の 1 つの編集を Vim が録る打った鍵へ（#1528・Vim 9.1 の feedkeys の実測）。Vim は
// 入力行の鍵もそのまま録り、`<Tab>` は文字の Tab（再生では 'wildchar' が効かず Tab が入る）、
// 上下は `<Up>` `<Down>`（再生でも履歴を引く）。Shift+Tab（`<S-Tab>`）は Nib の鍵の表に無いので
// 録らない。一覧の頁と端は面だけの編集で、`:` の行には届かない。
[[nodiscard]] std::optional<core::VimKey> recorded_edit_key(core::CommandEdit edit) noexcept
{
    switch (edit)
    {
    case core::CommandEdit::left:
        return core::VimKey{core::VimSpecialKey::arrow_left};
    case core::CommandEdit::right:
        return core::VimKey{core::VimSpecialKey::arrow_right};
    case core::CommandEdit::home:
        return core::VimKey{core::VimSpecialKey::home};
    case core::CommandEdit::end:
        return core::VimKey{core::VimSpecialKey::end};
    case core::CommandEdit::backspace:
        return core::VimKey{core::VimSpecialKey::backspace};
    case core::CommandEdit::erase:
        return core::VimKey{core::VimSpecialKey::delete_key};
    case core::CommandEdit::complete_next:
        return core::VimKey{core::VimCharacter{U'\t'}};
    case core::CommandEdit::history_previous:
        return core::VimKey{core::VimSpecialKey::arrow_up};
    case core::CommandEdit::history_next:
        return core::VimKey{core::VimSpecialKey::arrow_down};
    case core::CommandEdit::complete_previous:
    case core::CommandEdit::page_previous:
    case core::CommandEdit::page_next:
    case core::CommandEdit::list_first:
    case core::CommandEdit::list_last:
        return std::nullopt;
    }
    std::unreachable();
}

// 入力行へ入った文字列（打った文字・IME の確定・Ctrl+V の貼り付け）を 1 コードポイント 1 鍵に。
[[nodiscard]] std::vector<core::VimKey> typed_character_keys(std::string_view utf8)
{
    std::vector<core::VimKey> keys;
    core::Offset at{0};
    while (at.value < utf8.size())
    {
        keys.emplace_back(core::VimCharacter{core::code_point_at(utf8, at)});
        at = core::next_code_point(utf8, at);
    }
    return keys;
}

// Ex の答えが行を書き換える命令（`:s` の族・`:d` `:y`・`:g`・ADR 0168）なら、engine の 1 歩の
// 出どころ。ほかの答えは値なし。
[[nodiscard]] std::optional<VimStepSource> line_edit_source(const core::ExResult &result)
{
    if (result.substitute.has_value())
    {
        return VimStepSource{result.substitute.value()};
    }
    if (result.lines.has_value())
    {
        return VimStepSource{result.lines.value()};
    }
    return result.global.has_value() ? std::optional{VimStepSource{result.global.value()}}
                                     : std::nullopt;
}
} // namespace

// 今の鍵が打った鍵か（再生の中でも `map` の右辺でも :normal!（press_vim_keys が出どころを mapped
// にする）でもない・Vim の KeyTyped）。
bool EditorController::keys_typed() const noexcept
{
    return !replay_depth_.has_value() && core::vim_key_recorded_in_macro(unit_source_);
}

bool EditorController::records_command_line() const noexcept
{
    const auto &input = state_.command_input();
    return state_.mode() == core::EditMode::vim && input.has_value() &&
           std::holds_alternative<core::CommandLine>(input.value()) &&
           state_.vim().macro_recording.has_value() && keys_typed();
}

void EditorController::note_command_key() noexcept
{
    if (keys_typed())
    {
        command_line_typed_ = true;
    }
}

// 録画中に打った `:` で Ex の行が開いたら、`:` を積む前の録画を覚える（#1569 の
// L1）。確定（`<CR>`） と Esc（`<Esc>`）と空の行の
// Backspace（`<BS>`）は鍵を積んで覚えを消し、それ以外（Ctrl+P・
// タブの一覧・クリック・タブの切り替え）で行が捨てられたら settle_vim_line が録画を戻す。
void EditorController::note_recorded_line()
{
    recorded_line_.reset();
    const auto &recording = state_.vim().macro_recording;
    if (!records_command_line() || !recording.has_value())
    {
        return;
    }
    auto keys = recording.value().keys.owned_keys();
    if (!keys.empty())
    {
        keys.pop_back();
    }
    recorded_line_ = core::VimRecordedKeys::from(keys);
}

void EditorController::record_command_keys(std::span<const core::VimKey> keys)
{
    if (!records_command_line())
    {
        return;
    }
    auto vim = state_.vim();
    core::vim_macro_typed(vim, keys);
    state_ = state_.with_vim(std::move(vim));
}

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
    note_command_key();
    // 録画していなければ鍵の列を作らない（入力行と面の 1 打鍵ごとに確保しない・レビュー L1）。
    if (records_command_line())
    {
        record_command_keys(typed_character_keys(intent.utf8));
    }
    auto next = inserted_command(input.value(), intent.utf8);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    const bool overflowed = overflows_command(next.value());
    state_ = state_.with_command_input(std::move(next).value());
    // あふれた行は持ったまま、超えた所で知らせる（確定は丸ごと断る・#1568）。
    if (overflowed)
    {
        state_ = state_.with_command_message(core::ex_failure_message(core::ExFailure::too_long));
    }
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
    note_command_key();
    const auto recorded = recorded_edit_key(intent.edit);
    if (recorded.has_value())
    {
        record_command_keys(std::span<const core::VimKey>{&recorded.value(), 1U});
    }
    // 空の入力行の Backspace は取消（Ex と検索で同じ・ADR 0032 の決定 1）。
    // 設定一覧は候補を出したまま残る。
    const bool palette = std::holds_alternative<core::CommandPalette>(input.value());
    core::CommandEdit edit = intent.edit;
    if (!palette)
    {
        const auto line = input_line_of(input.value());
        edit = vim_line_edit(line, intent.edit);
        if (line.text.empty() && edit == core::CommandEdit::backspace)
        {
            recorded_line_.reset();
            cancel_command_input();
            return;
        }
    }
    const bool overflowed = overflows_command(input.value());
    CommandInput next = recalls_history(edit) ? recalled_command(input.value(), state_.ex_history(),
                                                                 state_.search_history(), edit)
                                              : edited_command(input.value(), edit);
    const bool fits = !overflows_command(next);
    state_ = state_.with_command_input(std::move(next));
    // Backspace などで上限の内へ戻れば、あふれの知らせを消す（そのまま確定できる・#1568）。
    if (overflowed && fits)
    {
        state_ = state_.with_command_message(std::nullopt);
    }
    reach_font_families();
    update_search_preview();
}

std::optional<core::VimRepeatFailure> EditorController::recalled_key(core::CommandEdit direction)
{
    const auto &input = state_.command_input();
    const bool moves = input.has_value() && recalls_command(input.value(), state_.ex_history(),
                                                            state_.search_history(), direction);
    accept(EditCommand{direction});
    if (moves)
    {
        return std::nullopt;
    }
    return core::VimRepeatFailure::not_moved;
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
    constexpr core::VimKey escape{core::VimSpecialKey::escape};
    record_command_keys(std::span<const core::VimKey>{&escape, 1U});
    recorded_line_.reset();
    cancel_command_input();
}

void EditorController::cancel_command_input()
{
    const auto &input = state_.command_input();
    // Vim の `/` `?` の取消だけ。通常モードの「検索: 」「行: 」は engine の待ちを持たない。
    const auto *line = input.has_value() ? std::get_if<core::SearchLine>(&input.value()) : nullptr;
    const bool searching = line != nullptr && vim_search_prompt(line->prompt());
    const bool ex_line =
        input.has_value() && std::holds_alternative<core::CommandLine>(input.value());
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
    // INSERT の `Ctrl-o` の命令として開いた Ex の行と検索の行は、取り消しても命令 1 つ（ADR 0182 の
    // 決定 8）。入力行の無い取消（知らせを閉じる）と面の取消は印に触らない。
    if (searching || ex_line)
    {
        resume_vim_insert();
    }
    // `d:` の入力行の取消で残ったオペレータは、意図の出口の settle_vim_line が捨てる（#1549）。
}

bool EditorController::window_request_pending() const noexcept
{
    return state_.close_request().has_value() || state_.operation_request().has_value() ||
           state_.closing();
}

// engine が開いた Vim の入力行（`:` `/` `?`・#1549・#1569）。確定（vim_command_operated と検索の
// 鍵）が閉じればオペレータの待ちは消え、INSERT の `Ctrl-o` の印は始め直しで進んでいる。待ちか、
// 始め直していない印が残ったまま Vim の入力行が開いていなければ、取消（Esc・Ctrl+C・空の行の
// Backspace）か、Ctrl+P・タブの一覧・クリック・タブの切り替えなどで入力行が閉じたか別の面に
// 置き換わったので、割り込みとして捨てる（Vim の clearop・ADR 0182 の決定 8）。録画中に打った
// `:` の行が確定も Esc も無く捨てられたら、`:` から後の録画を戻す（#1569 の L1）。入口ごとに
// 書かず、意図の出口のここの 1 か所で拾う。
void EditorController::settle_vim_line()
{
    if (!vim_line_open_ || vim_line_shown())
    {
        return;
    }
    vim_line_open_ = false;
    auto vim = state_.vim();
    if (recorded_line_.has_value() && vim.macro_recording.has_value())
    {
        vim.macro_recording.value().keys = recorded_line_.value();
    }
    recorded_line_.reset();
    const bool paused = vim.mode != core::VimMode::insert && core::vim_insert_paused(vim);
    state_ = state_.with_vim(vim.pending.has_value() || paused ? core::vim_interrupted(vim)
                                                               : std::move(vim));
}

// 開いている入力行が Vim の `:` の行か `/` `?` の行か（settle_vim_line が読む）。
bool EditorController::vim_line_shown() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return false;
    }
    const auto *search = std::get_if<core::SearchLine>(&input.value());
    return std::holds_alternative<core::CommandLine>(input.value()) ||
           (search != nullptr && vim_search_prompt(search->prompt()));
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

// Ex の評価の失敗は再生（`.`・マクロ・fixture の :normal!）を打ち切る（ADR 0022 の追補 #1526・
// Vim の emsg が typeahead を捨てる）。入力行に打った後ろの鍵が本文の鍵として流れない。
std::optional<core::VimRepeatFailure> EditorController::submit(const core::CommandLine &line)
{
    // オペレータ待ちの `:`（#1549）も Ex の行も、評価の後に始め直す（ADR 0182 の決定 8・
    // `<C-o>:set so=2<CR>` `<C-o>d:5<CR>`）。評価が失敗して再生を打ち切るときも、Vim の
    // restart_edit は残るので始め直す。
    const std::string text(line.text());
    const auto failure =
        state_.vim().pending.has_value() ? operator_command(text) : evaluate_command(text);
    resume_vim_insert();
    return failure;
}

// オペレータ待ちの `:` の確定（ADR 0022 の追補 #1549・Vim の nv_colon）。Ex は 1 回だけ評価する。
// 行へ跳ぶ命令（`:N` `:$`・`:.` `:.,.+N`）はキャレットを動かさずに行き先を engine へ渡し、
// オペレータの移動にする。ほかの命令は評価の答えの写し（ran_ex_result）を通し、断られたら
// （refused_operator_command）オペレータを捨てて refused を返す（再生も打ち切る）。成功なら
// operator_closed_in_place が動かないキャレットまでの空の範囲で閉じる。
std::optional<core::VimRepeatFailure> EditorController::operator_command(const std::string &text)
{
    close_command_input();
    if (text.empty())
    {
        return operator_closed_in_place(text);
    }
    const auto result = evaluated_ex(text);
    const auto edit = result.has_value() ? line_edit_source(result.value()) : std::nullopt;
    if (edit.has_value())
    {
        return operator_line_edited(text, edit.value());
    }
    const auto line = result.has_value() ? landing_line(result.value()) : std::nullopt;
    if (line.has_value())
    {
        return stepped_vim(VimStepSource{core::VimCommandMotion{text, line}});
    }
    if (refused_operator_command(text, result))
    {
        return core::VimRepeatFailure::refused;
    }
    return operator_closed_in_place(text);
}

// オペレータ待ちの `:s`（`d:s/a/b/`・ADR 0168 の追補 16）と `:d` `:y` `:g`（`d:3d`・段 3a）。Vim は
// 命令を先に流し、命令の前のキャレットから後のキャレットまでを exclusive な移動としてオペレータを
// 閉じる（`jd:s/a/X/` は `  b a` を `b X` に・`d:3d` は 3 行目を消してから 1 行目の頭から消した後の
// 3 行目の最初の非空白までを消す・実測）。命令が断られたら（E486 ほか）オペレータを捨てて再生を
// 打ち切る。
std::optional<core::VimRepeatFailure>
EditorController::operator_line_edited(const std::string &text, const VimStepSource &source)
{
    const core::Offset origin = state_.selection().caret;
    const auto failure = stepped_vim(source);
    if (failure.has_value())
    {
        state_ = state_.with_vim(core::vim_interrupted(state_.vim()));
        return failure;
    }
    if (!state_.vim().pending.has_value())
    {
        return std::nullopt;
    }
    return stepped_vim(VimStepSource{core::VimCommandMotion{text, std::nullopt, origin}});
}

// 答えを写し、断られた（ran_ex_result が refused を返した・評価の失敗も命令の要求の断りも・
// `:tabnext 9` など・Vim の did_emsg の clearopbeep）ならオペレータを捨てて真（#1549）。
bool EditorController::refused_operator_command(
    std::string_view text, const std::expected<core::ExResult, core::ExEvaluationFailure> &result)
{
    if (!ran_ex_result(text, result).has_value())
    {
        return false;
    }
    // 断りは Vim の clearopbeep で、INSERT の `Ctrl-o` の印は残す（#1569 の M1）。
    state_ = state_.with_vim(core::vim_operator_cleared(state_.vim()));
    return true;
}

// 跳ばない命令の成功の後（#1549）。動かないキャレットまでの空の範囲で閉じる（`d` は何も消さず `c`
// は INSERT へ入る・実測）。窓が後で行う要求（`:tabclose` の閉じる・一覧から頼んだ操作）は要求を
// 出してオペレータを捨て、タブを替えて待ちが消えていれば何もしない。
std::optional<core::VimRepeatFailure>
EditorController::operator_closed_in_place(const std::string &text)
{
    if (window_request_pending() || !state_.vim().pending.has_value())
    {
        state_ = state_.with_vim(core::vim_operator_cleared(state_.vim()));
        return std::nullopt;
    }
    return stepped_vim(VimStepSource{core::VimCommandMotion{text, std::nullopt}});
}

// Ex の答えがキャレットを跳ばす行（`:N` `:$` と範囲だけの `:.` `:.,.+N`）。`:'<,'>`
// と跳ばない命令は 値なし。
std::optional<core::ExLineRequest>
EditorController::landing_line(const core::ExResult &result) const
{
    if (result.line.has_value())
    {
        return result.line;
    }
    return result.relative_line.has_value() ? line_below_caret(result.relative_line.value())
                                            : std::nullopt;
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
        if (!refused_overflow())
        {
            replace_all_matches();
        }
        return;
    }
    static_cast<void>(confirmed_command());
}

// 確定の意図（Enter）だけが入力を履歴に足す。Esc は取消で、再生の中の <Esc> の確定
// （submitted_command を直接呼ぶ）も足さない（#725・ADR 0022 決定 2 の追補）。足すかを決めるのは
// remember の 1 か所（Ex の行は打った鍵が届いたときだけ・#1528 のレビュー M1）。
std::optional<core::VimRepeatFailure> EditorController::confirmed_command()
{
    note_command_key();
    constexpr core::VimKey enter{core::VimSpecialKey::enter};
    record_command_keys(std::span<const core::VimKey>{&enter, 1U});
    recorded_line_.reset();
    // あふれた行は履歴にも足さない（呼び出して確定すると切れた行になる・#1568）。
    if (refused_overflow())
    {
        return core::VimRepeatFailure::refused;
    }
    const auto &open = state_.command_input();
    if (open.has_value())
    {
        const CommandInput input = open.value();
        std::visit([this](const auto &value) { this->remember(value); }, input);
    }
    return submitted_command();
}

// Vim の getcmdline は `:` の行を、行の鍵が 1 つでも打たれたとき（some_key_typed）だけ履歴に
// 足す。再生（`@a` `.`）・`map` の右辺・:normal! だけで確定した行は足さない（Vim 9.1 の実測・
// #1528 のレビュー M1・D:/NeNeNib/scripts/20261112/c10b/h.vim）。
void EditorController::remember(const core::CommandLine &line)
{
    if (!command_line_typed_)
    {
        return;
    }
    state_ = state_.with_ex_history(state_.ex_history().remembered(line.text()));
}

// 「検索: 」の語も検索の履歴の 1 本へ（入力行の履歴は共用・ADR 0174 の決定 3）。
// 「行: 」と「置換後: 」は足さない。検索の行は打った鍵が無くても足す（Vim の getcmdline の
// `histype == HIST_SEARCH`・再生・`map` の右辺・:normal! の `/` も履歴に入る実測・#1528）。
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
    if (refused_overflow())
    {
        return core::VimRepeatFailure::refused;
    }
    // 入力行は state_ が持つので、写し先が状態を書き換える前に値ごと複製する。
    const CommandInput input = open.value();
    return std::visit([this](const auto &value) { return this->submit(value); }, input);
}

// あふれた入力の確定は丸ごと断る（ADR 0022 の追補 #1568）。切れた命令・pattern を実行しない。
// 取消と同じに入力行を閉じ（検索の preview の起点へ戻す・`/` の待ちを捨てる・INSERT の `Ctrl-o`
// を始め直す）、1 行知らせて鳴らす。本文・レジスタ・マーク・未保存の印には触れない。呼んだ道は
// refused を返して再生（`.`・マクロ・`map` の右辺・fixture）を打ち切る。入力が開いていない・
// あふれていなければ偽で何もしない。
bool EditorController::refused_overflow()
{
    const auto &open = state_.command_input();
    if (!open.has_value() || !overflows_command(open.value()))
    {
        return false;
    }
    cancel_command_input();
    reject(core::ex_failure_message(core::ExFailure::too_long));
    return true;
}

// 鳴らすかは呼んだ道で決め、文字列（E で始まるか）では決めない（ADR 0153 の決定 3）。Ex の命令の
// 要求の中から呼ぶと、その命令は断りとして再生を打ち切る（ran_ex_result・まとめ枝 #1563）。
void EditorController::reject(core::DisplayText message)
{
    state_ = state_.with_command_message(std::move(message));
    ports_.bell.ring();
    // 断りの数（ran_ex_result が、命令の要求が後で断ったことを Vim の did_emsg として読む・
    // #1549・まとめ枝 #1563）。
    ++refusals_;
}

// 評価の失敗と設定の保存の失敗は知らせて refused を返す（再生を打ち切る・ADR 0022 の追補 #1526）。
std::optional<core::VimRepeatFailure> EditorController::evaluate_command(std::string_view text)
{
    close_command_input();
    if (text.empty())
    {
        return std::nullopt;
    }
    return ran_ex_result(text, evaluated_ex(text));
}

// Ex の評価の 1 本（Ex の行とオペレータ待ちの `:` が同じ窓の状態を渡す・#1525・#1549）。
std::expected<core::ExResult, core::ExEvaluationFailure>
EditorController::evaluated_ex(std::string_view text) const
{
    return core::evaluate_ex(text, state_.settings(),
                             core::ExWindowState{state_.appearance(), state_.vim().highlight,
                                                 state_.vim().incsearch, state_.vim().wrapscan},
                             state_.themes());
}

// 評価した Ex の答えを写す（evaluate_command とオペレータ待ちの `:` が共用する 1 本・#1549）。
// 断りは評価の失敗も、命令の要求が後で断った（reject を通った・`:tabnext 9` `:tabclose` の最後の
// タブ・`:w` の E32 など）のも refused の 1 本で返す（Vim の emsg が先読みを捨てて再生を打ち切り、
// オペレータ待ちでは clearopbeep する・ADR 0022 の追補 #1525 / #1549・まとめ枝 #1563）。
std::optional<core::VimRepeatFailure> EditorController::ran_ex_result(
    std::string_view text, const std::expected<core::ExResult, core::ExEvaluationFailure> &result)
{
    if (!result)
    {
        reject(core::ex_failure_message(result.error(), text));
        return core::VimRepeatFailure::refused;
    }
    // `:s` の族と `:d` `:y` `:g` は engine の 1 歩（ADR 0168 の決定 1・段 3a）。E486 などの失敗は
    // 再生を打ち切る。
    const auto edit = line_edit_source(result.value());
    if (edit.has_value())
    {
        return stepped_vim(edit.value());
    }
    const std::size_t refusals = refusals_;
    const bool requested = window_request_pending();
    if (ran_ex_request(result.value()))
    {
        // 再生の中で窓の要求（閉じる・操作）を立てた命令は、要求を残したまま再生を打ち切る。要求は
        // 鍵の列の後で窓が行うので、後ろの鍵が閉じる前の文書を変えない（#1569 の M2・Vim の
        // `:tabclose` は E784 で打ち切る）。打った鍵は今までどおり後ろを捨てない。
        const bool deferred = !keys_typed() && !requested && window_request_pending();
        return refusals_ == refusals && !deferred ? std::nullopt
                                                  : std::optional{core::VimRepeatFailure::refused};
    }
    // Vim が窓を決め直す option は、設定を写す前に今のキャレットの決め直しを頼む（#1524）。幅の
    // ある窓では続く persist_settings の clamp_scroll が新しい設定でその場で解く。
    if (result.value().rechecks_window)
    {
        follow_window_setting();
    }
    const auto &settings = result.value().settings;
    if (settings.has_value() && !persist_settings(settings.value()))
    {
        reject(core::DisplayText::parse("Settings could not be saved").value());
        return core::VimRepeatFailure::refused;
    }
    apply_unsaved_ex(result.value());
    return noted_ex_result(text, result.value());
}

// 写した答えの知らせ。複数の引数の `:set` が途中で止まった（#1561）なら、前の引数の分は写したまま、
// Vim の do_set の emsg と同じく知らせて断る（再生を打ち切る）。
std::optional<core::VimRepeatFailure>
EditorController::noted_ex_result(std::string_view text, const core::ExResult &result)
{
    state_ = state_.with_command_message(result.message);
    if (result.stopped.has_value())
    {
        reject(core::ex_failure_message(result.stopped.value(), text));
        return core::VimRepeatFailure::refused;
    }
    return std::nullopt;
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
    if (result.relative_line.has_value())
    {
        jump_below_caret(result.relative_line.value());
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
    // `:hardcopy` は F1 の一覧の「印刷」と同じ意図へ渡す（ADR 0177 の決定 1）。
    if (result.print.has_value())
    {
        run_print_request(result.print.value());
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
    // 'wrapscan' も Vim の状態で、保存しない（#1561）。
    const auto &wrapscan = result.wrapscan;
    if (wrapscan.has_value())
    {
        core::VimState vim = state_.vim();
        vim.wrapscan = wrapscan.value();
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

// `:set language=<名前>` `:set language?` `:set language&`（ADR 0123 の追補 2026-11-13 の決定 2・
// 8）。選んだ値は設定 v4 の ui_language へ既存の保存の入口で書き、解決し直して状態に置く（frame
// の文はその場で変わる）。OS を観測するのは system を選んだときだけ（決定案 6）。保存できなければ
// 断って言語を変えない（`:set lsp` と同じ）。問い（`?`）は読むだけで保存を通さない。どの形も今の
// 希望値を 1 行で知らせ、希望値と解決済みの値が違う名前なら括弧で添える（`language=system (ja)`・
// `language=ko (en)`）。
void EditorController::answer_display_language(const core::ExDisplayLanguageRequest &request)
{
    auto settings = state_.settings();
    if (request.chosen.has_value())
    {
        settings.ui_language = request.chosen.value();
        if (!persist_settings(settings))
        {
            reject(core::DisplayText::parse("Settings could not be saved").value());
            return;
        }
        const bool follows_windows = settings.ui_language == core::DisplayLanguageChoice::system;
        state_ = state_.with_display_language(core::resolved_language(
            settings.ui_language, follows_windows ? ports_.ui_language.observe() : std::nullopt));
    }
    std::string shown = "language=";
    shown += core::display_language_choice_name(settings.ui_language);
    if (core::display_language_choice_name(settings.ui_language) !=
        core::display_language_code(state_.display_language()))
    {
        shown += " (";
        shown += core::display_language_code(state_.display_language());
        shown += ")";
    }
    state_ = state_.with_command_message(core::DisplayText::parse(shown).value());
}

// `:N` は鍵の `{N}G` と同じ engine の着地を同じ写しの道で写す（2 本にしない）。着地は必ず本文の中で
// 失敗しないので、断りの知らせも警告音も無い（Vim も `:8000` は黙って最後の行へ）。
void EditorController::jump_to_line(const core::ExLineRequest &request)
{
    static_cast<void>(stepped_vim(VimStepSource{request}));
}

// 範囲だけの `:.` `:.,.+N` はキャレットの行から N 行下へ `:N` と同じ着地（Vim の
// ex_range_without_command）。`:'<,'>`（below が空）は動かさず、知らせも出さない（ADR 0022 の
// 追補 #1526）。行の番号は size_t の最大に飽和し、本文の外は engine が最後の行へ寄せる。
void EditorController::jump_below_caret(const core::ExRelativeLineRequest &request)
{
    const auto line = line_below_caret(request);
    if (line.has_value())
    {
        jump_to_line(line.value());
    }
}

// `:.,.+N` の行き先（キャレットの行の N 行下・`'<,'>` は値なし）。跳ぶ道とオペレータ待ちの `:`
// （#1549）が同じ行を引く。
std::optional<core::ExLineRequest>
EditorController::line_below_caret(const core::ExRelativeLineRequest &request) const
{
    if (!request.below.has_value())
    {
        return std::nullopt;
    }
    const std::size_t line = state_.text().position_of(state_.selection().caret).line.value;
    const std::size_t below = request.below.value();
    constexpr std::size_t most = std::numeric_limits<std::size_t>::max();
    return core::ExLineRequest{below > most - line ? most : line + below};
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
    auto parsed = core::VimPattern::parse(
        core::literal_pattern(typed, core::VimSearchDirection::forward),
        core::VimSearchDirection::forward, core::search_case(state_.settings()));
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
    const auto edit = core::substituted_all(state_.text(), pattern.value(), find.replacement,
                                            core::maximum_document_bytes);
    if (!edit.has_value())
    {
        // 写し先が足りなければここでコンパイルが落ちる（CPP-002）。
        switch (edit.error())
        {
        case core::SubstituteFailure::no_match:
            static_cast<void>(hop_find(find.pattern, core::VimSearchDirection::forward, true));
            return;
        // 通常モードの語は literal で群を持たないので too_complex にはならない。来たら同じく断る。
        case core::SubstituteFailure::too_large:
        case core::SubstituteFailure::too_complex:
            // 全件の置換の後の文書が開ける上限を超えるときの知らせ（#1376・印刷の上限と同じ流儀・
            // 上限は定数から差し込む）。入力行の確定の断りなので鳴らす（ADR 0153 の決定 1）。
            reject(ui_notice(core::UiMessage::notice_replace_too_large,
                             {core::binary_size_text(core::maximum_document_bytes)}));
            return;
        }
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
    const auto failure = step_vim(core::VimKey{
        core::VimSearchPattern{core::literal_pattern(typed, relative), relative, from}});
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
        // 「行: 」が数でないときの知らせ（ADR 0174 の決定 8）。
        reject(ui_notice(core::UiMessage::notice_go_to_line_invalid));
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
    command_line_typed_ = false;
    if (opened)
    {
        state_ = state_.with_composition(std::nullopt);
    }
}
} // namespace nenenib::application
