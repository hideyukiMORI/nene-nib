#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CommandEdit.hpp"
#include "DisplayText.hpp"
#include "Edit.hpp"
#include "ExLineRequest.hpp"
#include "ExResult.hpp"
#include "SearchLine.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SplitLayout.hpp"
#include "TextCaret.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBlockEdit.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimClipboardText.hpp"
#include "VimInsertBlock.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimMarkEdits.hpp"
#include "VimMarkPosition.hpp"
#include "VimMatchRequest.hpp"
#include "VimMode.hpp"
#include "VimNavigate.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRemoveBlock.hpp"
#include "VimReplaceBlock.hpp"
#include "VimReplay.hpp"
#include "VimRoundTrip.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchNotice.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimStep.hpp"
#include "VimStepSource.hpp"
#include "VimSwitchTab.hpp"
#include "VimTabs.hpp"
#include "VimVisualReselect.hpp"
#include "VimWindowCommand.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
using detail::measured;
using detail::previewed_line;
using detail::typed_pattern;
using detail::with_document_newlines;

namespace
{
// 1 鍵の失敗（Issue #983）。鍵が自分で失敗しなければ、その鍵が返した再生（`.` `@`）を打ち切った
// 失敗を受け取って消す。Vim の :normal! は再生の中の失敗で `.` の後ろの鍵も捨てる（実測）。
[[nodiscard]] std::optional<core::VimRepeatFailure>
step_failure(std::optional<core::VimRepeatFailure> own,
             std::optional<core::VimRepeatFailure> &replayed) noexcept
{
    return own.has_value() ? own : std::exchange(replayed, std::nullopt);
}

// INSERT を抜けた位置を `'^` に置く（ADR 0152 の追補・#1039）。engine は効果を写した後の
// キャレットからの右への差で返すので、効果を写した後に置き、写した編集の行のずれは掛けない。
// 差が無い鍵（大半の鍵）では状態を写さない。
void place_insert_left(EditorState &state, std::optional<std::size_t> left)
{
    if (!left.has_value())
    {
        return;
    }
    core::VimMarks marks = state.vim_marks();
    marks.last_insert = core::vim_mark_position_of(
        state.text(), core::Offset{state.selection().caret.value + left.value()});
    state = state.with_vim_marks(std::move(marks));
}

// engine のキャレットは LF で数えた位置なので、直したあとの本文の上へずらす
// （CRLF は改行 1 つにつき 1 バイト長い）。数えるのは貼る位置からキャレットまでの改行だけ。
[[nodiscard]] core::Offset caret_after_insert(core::Offset at, std::string_view utf8,
                                              core::Offset caret, std::size_t newline_bytes)
{
    const auto inside = static_cast<std::ptrdiff_t>(caret.value - at.value);
    const auto newlines =
        static_cast<std::size_t>(std::count(utf8.begin(), utf8.begin() + inside, '\n'));
    return core::Offset{caret.value + newlines * (newline_bytes - 1)};
}

// 確定の検索の鍵と同じ vim_find_match 1 本で探した当たりの位置（ADR 0041 の決定 1）。
// 不一致は当たり無しで、報せは出さない。
[[nodiscard]] std::optional<core::TextPosition> found_match(const core::TextBuffer &text,
                                                            const core::VimPattern &pattern,
                                                            const core::VimMatchRequest &request)
{
    const auto hit = core::vim_find_match(text, pattern, request);
    if (!hit)
    {
        return std::nullopt;
    }
    return hit.value().position;
}

// 入力中のパターンの次の当たり。検索の起点から入力行の向きへ、確定の鍵と同じ回数だけ探す
// （ADR 0041 の決定 3・ADR 0043 の決定 1）。解析の失敗と不一致は当たり無し。
[[nodiscard]] std::optional<core::TextPosition> previewed_match(const EditorState &state,
                                                                core::TextPosition from)
{
    const core::SearchLine *line = previewed_line(state);
    if (line == nullptr)
    {
        return std::nullopt;
    }
    const auto pattern = typed_pattern(*line);
    if (!pattern.has_value())
    {
        return std::nullopt;
    }
    return found_match(
        state.text(), pattern.value(),
        core::VimMatchRequest{from, line->direction(), core::vim_search_count(state.vim())});
}

// 境界が文字の途中か CRLF の間なら、覆う範囲を外へ 1 バイト広げる向きに動かせるか。
[[nodiscard]] bool splits_a_character(std::string_view text, std::size_t at) noexcept
{
    if (at == 0 || at >= text.size())
    {
        return false;
    }
    const auto byte = static_cast<unsigned char>(text[at]);
    return (byte & 0xC0U) == 0x80U || (text[at] == '\n' && text[at - 1] == '\r');
}

// 先頭の共通部分の長さ。文字と CRLF を割らない所まで戻す。
[[nodiscard]] std::size_t common_head(std::string_view before, std::string_view after) noexcept
{
    std::size_t head = 0;
    const std::size_t shorter = std::min(before.size(), after.size());
    while (head < shorter && before[head] == after[head])
    {
        ++head;
    }
    while (head > 0 && (splits_a_character(before, head) || splits_a_character(after, head)))
    {
        --head;
    }
    return head;
}

// 末尾の共通部分の長さ（先頭の共通部分とは重ねない）。文字と CRLF を割らない所まで戻す。
[[nodiscard]] std::size_t common_tail(std::string_view before, std::string_view after,
                                      std::size_t head) noexcept
{
    std::size_t tail = 0;
    const std::size_t room = std::min(before.size(), after.size()) - head;
    while (tail < room && before[before.size() - 1 - tail] == after[after.size() - 1 - tail])
    {
        ++tail;
    }
    while (tail > 0 && (splits_a_character(before, before.size() - tail) ||
                        splits_a_character(after, after.size() - tail)))
    {
        --tail;
    }
    return tail;
}

// 前後の本文の違う所を覆う 1 つの Edit（ADR 0046 の決定 3）。同じ本文なら空。restore は畳む
// 単位の最初の編集の値を呼ぶ側が渡す（ADR 0052 の決定 3）。
[[nodiscard]] std::optional<core::Edit> covering_edit(std::string_view before,
                                                      std::string_view after, core::Offset restore)
{
    if (before == after)
    {
        return std::nullopt;
    }
    const std::size_t head = common_head(before, after);
    const std::size_t tail = common_tail(before, after, head);
    return core::Edit{core::Offset{head},
                      std::string(before.substr(head, before.size() - head - tail)),
                      std::string(after.substr(head, after.size() - head - tail)), restore};
}

// Vim の問いの往復の契約の破れの知らせ（ADR 0133 の追補 2026-10-18・#665）。鍵は効かない。
constexpr std::string_view inconsistent_layout_notice = "内部の不整合: 画面行の問い（鍵は無効）";
// engine を呼ぶ 1 か所の写し（Issue #1023）。鍵は vim_step・`:N` の行は vim_line_jumped。
[[nodiscard]] core::VimStep engine_step(const core::VimState &state,
                                        const core::VimEditorView &view, const core::VimKey &key)
{
    return core::vim_step(state, view, key);
}

[[nodiscard]] core::VimStep engine_step(const core::VimState &state,
                                        const core::VimEditorView &view,
                                        const core::ExLineRequest &request)
{
    return core::vim_line_jumped(state, view, request.line);
}

[[nodiscard]] core::VimStep engine_step(const core::VimState &state,
                                        const core::VimEditorView &view,
                                        const VimStepSource &source)
{
    return std::visit([&state, &view](const auto &chosen)
                      { return engine_step(state, view, chosen); }, source);
}

// engine が求めた行の画面行数を、行の順の列へ足す。同じ行を 2 度求めたら往復の契約の破れ
// （inconsistent・ADR 0133 の追補 2026-10-18）。
[[nodiscard]] std::expected<void, TextLayoutFailure>
measured_rows(std::vector<core::VimLineRows> &rows, core::LineNumber line, VisualTextView &text)
{
    const auto unasked = vim_rows_unasked(rows, line);
    if (!unasked)
    {
        return unasked;
    }
    const auto found = std::ranges::lower_bound(
        rows, line.value, {}, [](const core::VimLineRows &value) { return value.line.value; });
    const auto count = text.rows(line);
    if (!count)
    {
        return std::unexpected(count.error());
    }
    rows.insert(found, core::VimLineRows{line, count.value()});
    return {};
}

// engine が求めた行の中の画面行を、答えの列へ足す（ADR 0144 の決定 2）。同じ答えを 2 度足したら
// 答えが問いに当たっていない往復の契約の破れ（inconsistent）。row_span 自身の失敗はそのまま。
[[nodiscard]] std::expected<void, TextLayoutFailure>
measured_span(std::vector<core::VimRowSpan> &spans, const core::VimRowRequest &request,
              VisualTextView &text)
{
    const auto span = text.row_span(request);
    if (!span)
    {
        return std::unexpected(span.error());
    }
    const auto unanswered = vim_span_unanswered(spans, span.value());
    if (!unanswered)
    {
        return unanswered;
    }
    spans.push_back(span.value());
    return {};
}
} // namespace

// fixture と契約の harness が打つ 1 鍵（ADR 0048 の決定 8）。入力行の写しは再生と同じ
// command_key の 1 か所を通り、打った鍵なので <Esc> は取消のまま。
EditorFrame EditorController::press_vim_key(const core::VimKey &key)
{
    settle_tab_walk_before(false);
    begin_intent(false);
    static_cast<void>(deliver_vim_key(key));
    fit_command_rows();
    return frame();
}

// fixture の harness が Vim の :normal! の意味で鍵の列を流す口（Issue #230）。1 鍵の流し方は
// press_vim_key と同じで、鍵が閉じた失敗で終わったら残りの鍵を流さない（再生の打ち切りと同じ
// deliver_vim_key の返り値で決める）。
EditorFrame EditorController::press_vim_keys(std::span<const core::VimKey> keys)
{
    for (const core::VimKey &key : keys)
    {
        settle_tab_walk_before(false);
        begin_intent(false);
        const bool failed = deliver_vim_key(key).has_value();
        fit_command_rows();
        if (failed)
        {
            break;
        }
    }
    return frame();
}

// undo と Ctrl-r の編集でマークを直す（ADR 0152 の決定 6）。最後の変更は編集の行の行頭（Vim の
// u_undoredo の changed_lines）。edit は今の本文に効く向きの編集。
void EditorController::undone_marks(const core::Edit &edit)
{
    auto marks = core::marks_adjusted(state_.vim_marks(), state_.text(), edit);
    marks.last_change = core::VimMarkPosition{state_.text().position_of(edit.at).line, 0};
    state_ = state_.with_vim_marks(std::move(marks));
}

// 窓は入力行の鍵を VimKeyPress ではなく CommandText / EditCommand / SubmitCommand /
// CancelCommand で送る。入力行（設定一覧を含む）が開いているあいだに届いた Vim の鍵は捨てる。
void EditorController::accept(const VimKeyPress &intent)
{
    if (state_.command_input().has_value())
    {
        return;
    }
    static_cast<void>(step_vim(intent.key));
}

// 再生の鍵の口（ADR 0048 の決定 8）。入力行が開いていれば鍵を窓と同じ入力行の intent に写し、
// 閉じていれば engine へ流す。確定した検索の鍵は入力行を経ずに engine へ。入力行の鍵は
// 失敗しない。失敗を返すのは engine の鍵（入力行の確定が送る検索の鍵を含む）だけ。
std::optional<core::VimRepeatFailure> EditorController::deliver_vim_key(const core::VimKey &key)
{
    if (!state_.command_input().has_value())
    {
        return step_vim(key);
    }
    return std::visit([this](const auto &value) { return this->command_key(value); }, key);
}

std::optional<core::VimRepeatFailure> EditorController::command_key(const core::VimCharacter &key)
{
    std::string utf8;
    core::append_utf8(utf8, key.code);
    accept(CommandText{std::move(utf8)});
    return std::nullopt;
}

// 入力行での特殊鍵の写し（ADR 0048 の決定 8）。窓と同じ EditCommand の写しで、上下（履歴）と
// 入力行に意味の無い鍵は捨てる。再生の中の <Esc> は Vim の c_<Esc> と同じに確定する。
std::optional<core::VimRepeatFailure> EditorController::command_key(core::VimSpecialKey key)
{
    switch (key)
    {
    case core::VimSpecialKey::enter:
        return confirmed_command();
    case core::VimSpecialKey::escape:
        if (replay_depth_.has_value())
        {
            return submitted_command();
        }
        accept(CancelCommand{});
        return std::nullopt;
    case core::VimSpecialKey::backspace:
        accept(EditCommand{core::CommandEdit::backspace});
        return std::nullopt;
    case core::VimSpecialKey::arrow_left:
        accept(EditCommand{core::CommandEdit::left});
        return std::nullopt;
    case core::VimSpecialKey::arrow_right:
        accept(EditCommand{core::CommandEdit::right});
        return std::nullopt;
    case core::VimSpecialKey::home:
        accept(EditCommand{core::CommandEdit::home});
        return std::nullopt;
    case core::VimSpecialKey::end:
        accept(EditCommand{core::CommandEdit::end});
        return std::nullopt;
    case core::VimSpecialKey::arrow_up:
    case core::VimSpecialKey::arrow_down:
    case core::VimSpecialKey::control_r:
    case core::VimSpecialKey::page_up:
    case core::VimSpecialKey::page_down:
    case core::VimSpecialKey::control_d:
    case core::VimSpecialKey::control_u:
    case core::VimSpecialKey::control_f:
    case core::VimSpecialKey::control_b:
    case core::VimSpecialKey::control_v:
    case core::VimSpecialKey::control_w:
        return std::nullopt;
    }
    std::unreachable();
}

std::optional<core::VimRepeatFailure>
EditorController::command_key(const core::VimSearchPattern &key)
{
    return step_vim(core::VimKey{key});
}

// 写しを読みうる状態と鍵なら、鍵を流す直前に OS の本文を読んで engine に置く（ADR 0051 の決定 3）。
// `:set clipboard=unnamed` の `p` `P` は無名へ、ほかは `"+` の写しへ置く（追補（#848）の決定 D）。
// 読めなければ空のレジスタを置き、engine が空のレジスタと同じに拒む。
void EditorController::load_vim_clipboard(const core::VimKey &key)
{
    if (!core::vim_reads_clipboard(state_.vim(), key))
    {
        return;
    }
    const auto pasted = ports_.clipboard.read();
    auto value = pasted ? core::vim_register_of_clipboard(pasted.value())
                        : core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized};
    state_ = state_.with_vim(core::vim_reads_unnamed_clipboard(state_.vim(), key)
                                 ? core::vim_unnamed_loaded(state_.vim(), std::move(value))
                                 : core::vim_clipboard_loaded(state_.vim(), std::move(value)));
}

// `"+` へ書いた本文は文書の改行で OS へ出す（ADR 0051 の決定 7・Ctrl+C と同じ形）。書けなくても
// 本文と無名は engine の結果のまま（copy_selection と同じ）で、知らせだけ出す
// （ADR 0134 の決定 3）。
void EditorController::send_vim_clipboard(const std::optional<core::VimRegister> &written)
{
    if (!written.has_value())
    {
        return;
    }
    static_cast<void>(clipboard_written(with_document_newlines(
        core::vim_clipboard_text_of(written.value()), core::newline_of(state_.line_ending()))));
}

std::optional<core::VimRepeatFailure> EditorController::step_vim(const core::VimKey &key)
{
    load_vim_clipboard(key);
    return stepped_vim(VimStepSource{key});
}

std::optional<core::VimRepeatFailure> EditorController::stepped_vim(const VimStepSource &source)
{
    const core::VimMode before = state_.vim().mode;
    const auto measured = measured_vim_step(source);
    if (!measured)
    {
        layout_failure_ = measured.error();
        // 契約の破れは描画の作り直しでは直らないので、鍵を拒否して 1 行知らせる（ADR 0133 の
        // 追補 2026-10-18）。layout を作れない失敗は ui が作り直す（知らせは出さない）。
        switch (measured.error())
        {
        case TextLayoutFailure::inconsistent:
            state_ = state_.with_command_message(
                core::DisplayText::parse(inconsistent_layout_notice).value());
            break;
        case TextLayoutFailure::unavailable:
            break;
        }
        return core::VimRepeatFailure::refused;
    }
    const auto &step = measured.value();
    state_ = state_.with_vim(step.next);
    // INSERT の出入りが undo の区切り（ADR 0012 の決定 6 / ADR 0009 の決定 3 の Vim 側）。
    if (before != step.next.mode && before != core::VimMode::insert)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    // オペレータと VISUAL の戻り先（ADR 0052 の決定 7）。効果を写す前にキャレットを置くので、
    // replace が覚える戻り先は Vim の保存の瞬間のカーソルと同じになる。スクロールは効果の後で追う。
    if (step.restore.has_value())
    {
        state_ = state_.with_selection(core::collapsed_at(step.restore.value()));
    }
    // マークは効果を写す前に置く。飛ぶ鍵が置いた前の位置もオペレータの編集で直る（ADR 0152 の
    // 決定 2・6・Vim の setpcmark の後の mark_adjust）。
    adopt_vim_marks(step.marks);
    // 写し先が足りなければここでコンパイルが落ちる＝効果が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->perform(value); }, step.effect);
    place_insert_left(state_, step.insert_left);
    // 検索の報せは Ex の結果と同じ 1 本に出し、次の入力で消える（ADR 0032 の決定 5）。
    if (step.notice.has_value())
    {
        state_ = state_.with_command_message(core::vim_search_message(step.notice.value()));
    }
    // 書けなかった知らせは検索の報せの後に置き、同じ鍵で両方起きたら書けなかった方を見せる。
    send_vim_clipboard(step.clipboard);
    if (before == core::VimMode::insert && before != step.next.mode)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    if (!state_.command_input().has_value())
    {
        settle_vim_caret();
    }
    // 鍵の終わりに、動かなかった・消えた前の位置を戻す（決定 5・Vim の checkpcmark）。
    check_vim_marks();
    // 再生（`.` `@`）の中の鍵が失敗したら、再生を返した鍵の失敗にする（Issue #983）。
    return step_failure(step.failure, replay_failure_);
}

// 飛ぶ鍵の後だけ前の位置を確かめる（決定 5）。飛ばなかった鍵は位置を引かない。
void EditorController::check_vim_marks()
{
    if (!state_.vim_marks().prior_context.has_value())
    {
        return;
    }
    state_ = state_.with_vim_marks(core::vim_marks_checked(
        state_.vim_marks(), core::vim_mark_position_of(state_.text(), state_.selection().caret)));
}

void EditorController::adopt_vim_marks(const std::optional<core::VimMarks> &marks)
{
    if (marks.has_value())
    {
        state_ = state_.with_vim_marks(marks.value());
    }
}

void EditorController::settle_vim_caret()
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return;
    case core::EditMode::vim:
        break;
    }
    switch (state_.vim().mode)
    {
    // VISUAL のキャレットは行の内容の終わりにも載る。寄せるのは NORMAL へ戻るときだけ
    // （ADR 0018 の決定 6）。選択も畳まない。
    case core::VimMode::insert:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return;
    case core::VimMode::normal:
        break;
    }
    move_caret_to(core::vim_resting_caret(state_.text(), state_.selection().caret),
                  core::SelectionAnchoring::collapse);
}

// Vim の効果を本文に写すときの undo の区切り（ADR 0015 の決定 5）。INSERT にいるあいだの編集は
// 直前の Edit に吸収して 1 単位にし、NORMAL の編集（x d p）は単位を切る。
// VimInsertAtはEsc時の反復も持つため、明示した境界を使う（ADR 0028）。
core::EditBoundary EditorController::vim_boundary() const noexcept
{
    switch (state_.vim().mode)
    {
    case core::VimMode::insert:
        return core::EditBoundary::absorb;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return core::EditBoundary::separate;
    }
    std::unreachable();
}

void EditorController::perform(const core::VimNoEffect &) {}

void EditorController::perform(const core::VimOpenCommandLine &)
{
    state_ = state_.with_command_input(
        core::CommandLine::empty(state_.themes()).with_fonts(given_fonts()));
}

void EditorController::perform(const core::VimOpenSearch &effect)
{
    const ScrollState origin = settled_scroll();
    state_ = state_.with_command_input(core::SearchLine::opened(effect.direction));
    state_ = state_.with_search_preview(
        SearchPreview{std::nullopt, origin, state_.text().position_of(state_.selection().caret)});
}

// 入力行が変わるたびに 1 か所で呼ぶ（ADR 0041 の決定 3）。画面は毎回 origin から数え直すので、
// 打ち直しても同じ入力なら同じ位置に見える。当たりが無ければ origin のまま。
void EditorController::update_search_preview()
{
    const auto &preview = state_.search_preview();
    if (!preview.has_value())
    {
        return;
    }
    const ScrollState origin = preview.value().origin;
    const core::TextPosition from = preview.value().from;
    const auto match = previewed_match(state_, from);
    restore_search_origin(origin);
    state_ = state_.with_search_preview(SearchPreview{match, origin, from});
    if (match.has_value())
    {
        follow_position(match.value());
    }
}

// Ctrl-G / Ctrl-T（ADR 0043 の決定 2）。検索の入力行が開いていて incsearch が当たりを見せている
// ときだけ動く。向きは本文の順（Ctrl-G は下・Ctrl-T は上）で `/` `?` に依らない。確定の鍵は起点から
// 検索の向きへ回数ぶん探すので、起点は新しい当たりから検索の向きの逆へ同じ回数だけ戻った当たりに
// 置く（`/` の Ctrl-G では今の当たりになる）。折り返しは vim_find_match
// の規則のままで報せは出さない。
void EditorController::accept(const StoreVimRegister &intent)
{
    state_ = state_.with_vim(core::vim_register_stored(state_.vim(), intent.name, intent.value));
}

void EditorController::accept(const SearchHop &intent)
{
    // 変換中の鍵は IME のもの。検索の行の変換中は当たりを動かさない（ADR 0061 の #635 の追記）。
    // 変換中かは ui と同じ述語 is_composing の 1 本で見る。
    if (is_composing())
    {
        return;
    }
    const core::SearchLine *line = previewed_line(state_);
    const auto preview = state_.search_preview();
    if (line == nullptr || !preview.has_value() || !preview.value().match.has_value())
    {
        return;
    }
    const auto pattern = typed_pattern(*line);
    if (!pattern.has_value())
    {
        return;
    }
    const std::size_t count = core::vim_search_count(state_.vim());
    const auto match =
        found_match(state_.text(), pattern.value(),
                    core::VimMatchRequest{preview.value().match.value(), intent.relative, count});
    if (!match.has_value())
    {
        return;
    }
    const auto from =
        found_match(state_.text(), pattern.value(),
                    core::VimMatchRequest{match.value(), core::opposite(line->direction()), count});
    if (!from.has_value())
    {
        return;
    }
    state_ = state_.with_search_preview(SearchPreview{match, preview.value().origin, from.value()});
    follow_position(match.value());
}

// 見えている行数は入力中に窓の大きさで変わり得るので、戻すのは先頭行だけ。
void EditorController::restore_search_origin(const ScrollState &origin)
{
    const std::size_t visible = settled_scroll().visible_lines;
    state_ = state_.with_scroll(
        ScrollState{origin.first_visible, visible, origin.first_row, origin.horizontal});
}

// Vim の外から来た割り込み（クリック・Ctrl+Z・全選択・別経路の編集）。どれが割り込みかは
// ここが決め、何を捨てるかは engine の純関数 vim_interrupted が決める（Issue #92 / ARC-004）。
// undo の単位を切るのは履歴を持つこちらの仕事（ADR 0028 の決定 3）。
void EditorController::interrupt_vim_insert()
{
    if (state_.vim().mode == core::VimMode::insert)
    {
        state_ = state_.with_vim(core::vim_interrupted(state_.vim()))
                     .with_history(state_.history().sealed());
    }
}

// Vim の鍵による移動。入力記録を残すか捨てるかは engine が決めている（insert_moved が捨て、
// i a I A の入りは残す）ので、ここは undo の単位を切るだけにする（ARC-004）。
void EditorController::perform(const core::VimMoveTo &effect)
{
    if (state_.vim().mode == core::VimMode::insert)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    move_caret_to(effect.caret, core::SelectionAnchoring::collapse);
}

// 縦は engine の値を置く。横は first_column があればその leftcol を新しいキャレットの窓で
// 画素にして置き（追従は呼ばない）、無ければ今の値を保ってキャレットが見える最小の送りへ
// 1 回寄せる（ADR 0145 の決定 8・9）。横位置が求まらなければ、追従やスクロールの失敗と同じく
// layout_failure_ に記録して描画の失敗の道へ渡す（選択と縦の位置は置いたまま・横は前の値）。
void EditorController::perform(const core::VimNavigate &effect)
{
    const ScrollState before = settled_scroll();
    state_ = state_.with_selection(effect.selection)
                 .with_scroll(ScrollState{effect.first_visible, before.visible_lines,
                                          effect.first_row, before.horizontal});
    state_ = state_.with_history(state_.history().sealed());
    auto view = visual_view();
    const auto caret = core::TextCaret{state_.text().position_of(state_.selection().caret),
                                       state_.caret_affinity()};
    const auto horizontal =
        effect.first_column.has_value()
            ? view.horizontal_of_column(caret.position.line, effect.first_column.value())
            : view.followed_horizontal(caret);
    if (!horizontal)
    {
        layout_failure_ = horizontal.error();
        return;
    }
    auto scroll = state_.scroll();
    scroll.horizontal = horizontal.value();
    state_ = state_.with_scroll(scroll);
}

// VISUAL の選択（決定 3）。engine が決めた両端をそのまま置く。範囲にするのは表示と操作の側。
void EditorController::perform(const core::VimSelect &effect)
{
    state_ = state_.with_selection(effect.selection);
    follow_caret();
}

void EditorController::perform(const core::VimRemoveRange &effect)
{
    replace(effect.range, std::string_view{}, vim_boundary());
}

void EditorController::perform(const core::VimRemoveLines &effect)
{
    replace(effect.range, std::string_view{}, core::EditBoundary::separate);
    // 行を消したあとの Vim のキャレットは、その位置に来た行の最初の非空白。
    move_caret_to(core::vim_first_non_blank(state_.text(), effect.range.begin),
                  core::SelectionAnchoring::collapse);
}

void EditorController::perform(const core::VimInsertString &effect)
{
    replace(core::selection_range(state_.selection()), effect.utf8, vim_boundary());
}

void EditorController::perform(const core::VimNewLine &)
{
    replace(core::selection_range(state_.selection()), core::newline_of(state_.line_ending()),
            vim_boundary());
}

// p/P・o/O・Esc時の反復を同じ改行変換とreplaceへ流す（ADR 0028）。
void EditorController::perform(const core::VimInsertAt &effect)
{
    const std::string_view newline = core::newline_of(state_.line_ending());
    replace(core::OffsetRange{effect.at, effect.at}, with_document_newlines(effect.utf8, newline),
            effect.boundary);
    move_caret_to(caret_after_insert(effect.at, effect.utf8, effect.caret, newline.size()),
                  core::SelectionAnchoring::collapse);
}

void EditorController::perform(const core::VimReplaceRange &effect)
{
    const std::string_view newline = core::newline_of(state_.line_ending());
    replace(effect.range, with_document_newlines(effect.utf8, newline),
            core::EditBoundary::separate);
    move_caret_to(caret_after_insert(effect.range.begin, effect.utf8, effect.caret, newline.size()),
                  core::SelectionAnchoring::collapse);
}

// 矩形の編集（ADR 0035 の決定 3）。行ごとの置き換えを、上の行から下の行までを覆う 1 つの
// 置き換えに畳んでから既存の replace へ流す。こうすると履歴に入る Edit が 1 つになり、矩形
// 全体が undo 1 単位になる（固定 Vim も 1 回の `u` で戻す・実測）。改行は文書の形へ直す。
void EditorController::apply_block(const std::vector<core::VimBlockEdit> &edits, core::Offset caret)
{
    if (edits.empty())
    {
        move_caret_to(caret, core::SelectionAnchoring::collapse);
        return;
    }
    const std::string_view newline = core::newline_of(state_.line_ending());
    const core::OffsetRange span{edits.front().range.begin, edits.back().range.end};
    std::string body;
    core::Offset at = span.begin;
    for (const core::VimBlockEdit &edit : edits)
    {
        body += state_.text().text_range(at, edit.range.begin);
        body += with_document_newlines(edit.utf8, newline);
        at = edit.range.end;
    }
    replace(span, body, core::EditBoundary::separate);
    move_caret_to(caret, core::SelectionAnchoring::collapse);
}

void EditorController::perform(const core::VimRemoveBlock &effect)
{
    apply_block(effect.edits, effect.caret);
}

void EditorController::perform(const core::VimReplaceBlock &effect)
{
    apply_block(effect.edits, effect.caret);
}

void EditorController::perform(const core::VimInsertBlock &effect)
{
    apply_block(effect.edits, effect.caret);
}

// `.`（ADR 0030 の決定 7）。前後で履歴を閉じるので、再生した命令が 1 つの undo 単位になる。
// VISUAL の記録なら、鍵を流す前に core が決めた範囲を VimSelect と同じ写しで置く
// （ADR 0033 の決定 4。engine が mode を VISUAL にしてあるので、ここは選択だけを置く）。
//
// `@` も同じ効果で来る（ADR 0046 の決定 3・4）。再生の鍵は 1 本の列に積んで先頭から流す。
// 再生の中で `@` や `.` がまた VimReplay を返したら、その鍵を列の先頭へ差し込む（Vim が
// レジスタの中身を先読みの頭へ入れるのと同じ順）。流している鍵が閉じた失敗で終わったら列の
// 残りをすべて捨てる（Vim の「エラーで残りの打鍵を捨てる」）。差し込みの深さが上限を超えたら
// 何も差し込まずに残りを捨てる。入れ子を C++ の呼び出しの深さにしないので、深い再帰でも
// スタックを食わない。
void EditorController::perform(const core::VimReplay &effect)
{
    state_ = state_.with_history(state_.history().sealed());
    if (effect.reselect.has_value())
    {
        state_ = state_.with_selection(core::vim_visual_reselect(
            state_.text(), state_.selection().caret, effect.reselect.value()));
        follow_caret();
    }
    if (replay_depth_.has_value())
    {
        queue_nested_replay(effect.keys, replay_depth_.value() + 1);
        return;
    }
    replay_history_ = state_.history();
    replay_text_ = state_.text();
    replay_queue_.assign(effect.keys.begin(), effect.keys.end());
    replay_depths_.assign(effect.keys.size(), 1);
    while (!replay_queue_.empty())
    {
        const core::VimKey key = replay_queue_.front();
        replay_depth_ = replay_depths_.front();
        replay_queue_.pop_front();
        replay_depths_.pop_front();
        const auto failure = deliver_vim_key(key);
        if (failure.has_value())
        {
            replay_failure_ = failure;
            replay_queue_.clear();
            replay_depths_.clear();
        }
    }
    replay_depth_ = std::nullopt;
    close_replayed_unit();
    replay_history_ = std::nullopt;
    replay_text_ = std::nullopt;
    state_ = state_.with_history(state_.history().sealed());
}

void EditorController::close_replayed_unit()
{
    if (!replay_history_.has_value() || !replay_text_.has_value())
    {
        return;
    }
    merge_replayed_edits(replay_history_.value(), replay_text_.value());
}

void EditorController::reopen_replayed_unit()
{
    if (!replay_history_.has_value())
    {
        return;
    }
    replay_history_ = state_.history();
    replay_text_ = state_.text();
}

void EditorController::queue_nested_replay(const std::vector<core::VimKey> &keys, std::size_t depth)
{
    if (depth > core::vim_replay_depth_limit)
    {
        replay_queue_.clear();
        replay_depths_.clear();
        state_ = state_.with_command_message(
            core::DisplayText::parse("E132: Macro depth is higher than 100").value());
        return;
    }
    replay_queue_.insert(replay_queue_.begin(), keys.begin(), keys.end());
    replay_depths_.insert(replay_depths_.begin(), keys.size(), depth);
}

// 再生は命令ごとに編集を積むが、`[count]@a` と `.` の全体を 1 回の `u` で戻すのが Vim と同じ
// （ADR 0046 の決定 3・Issue #176 の probe で実測）。履歴の 1 単位は連続した 1 つの Edit なので、
// 2 つ以上積んだときは、再生の前後の本文の違う所をまとめて覆う 1 つの Edit に置き換える
// （矩形の編集を 1 つの範囲の置換にする apply_block と同じ考え方）。
void EditorController::merge_replayed_edits(const core::EditHistory &before,
                                            const core::TextBuffer &text)
{
    // 畳んだ単位の戻り先は再生が積んだ最初の編集の restore（ADR 0052 の決定 3）。
    const auto first = state_.history().applied(before.position());
    if (state_.history().position() <= before.position() + 1 || !first.has_value())
    {
        return;
    }
    const auto edit = covering_edit(text.text(), state_.text().text(), first.value().restore);
    if (!edit.has_value())
    {
        state_ = state_.with_history(before.sealed());
        return;
    }
    state_ = state_.with_history(before.pushed(edit.value(), core::EditBoundary::separate));
}

// Vim の u は戻した単位が覚えた戻り先（最初の編集の瞬間のキャレット）へ置き、行末の 1 つ先などの
// 寄せは step_vim の最後の settle_vim_caret に任せる（ADR 0052 の決定 4）。
void EditorController::perform(const core::VimUndo &)
{
    const auto edit = state_.history().undo();
    undo_edit();
    if (edit.has_value())
    {
        move_caret_to(edit.value().restore, core::SelectionAnchoring::collapse);
    }
}

// Vim の Ctrl-r は同じ戻り先の行とバイトの桁を、やり直した後の本文の同じ行と桁へ写す（ADR 0052 の
// 決定 5）。
void EditorController::perform(const core::VimRedo &)
{
    const auto edit = state_.history().redo();
    const core::TextBuffer before = state_.text();
    redo_edit();
    if (edit.has_value())
    {
        move_caret_to(core::vim_same_line_and_column(before, edit.value().restore, state_.text()),
                      core::SelectionAnchoring::collapse);
    }
}

void EditorController::perform(const core::VimSwitchTab &effect)
{
    accept(SwitchTab{effect.index});
}

// Vim の `Ctrl-w` は枠の鍵と同じ意図へ（ADR 0148 の決定 6・7）。2 枠なので隣は 1 つで、
// `h j k l` も `w` と同じく隣へ移る。`c` `q` は操作中の枠を閉じる（移ってからもう 1 つの枠を
// 閉じる）。1 枠では移る・閉じるは何もしない（Vim は最後の窓の `Ctrl-w c` を失敗にするが、Nib は
// 何もしない）。`o` はもう 1 つの枠を閉じる。
void EditorController::perform(const core::VimWindowCommand &command)
{
    switch (command)
    {
    case core::VimWindowCommand::split_horizontal:
        accept(SplitPane{core::SplitLayout::horizontal});
        return;
    case core::VimWindowCommand::split_vertical:
        accept(SplitPane{core::SplitLayout::vertical});
        return;
    case core::VimWindowCommand::next:
    case core::VimWindowCommand::left:
    case core::VimWindowCommand::down:
    case core::VimWindowCommand::up:
    case core::VimWindowCommand::right:
        accept(SwitchPane{});
        return;
    case core::VimWindowCommand::close:
        if (state_.split().other.has_value())
        {
            close_operated_pane();
        }
        return;
    case core::VimWindowCommand::only:
        accept(ClosePane{});
        return;
    }
    std::unreachable();
}

std::expected<core::VimStep, TextLayoutFailure>
EditorController::measured_vim_step(const VimStepSource &source)
{
    std::vector<core::VimLineRows> rows;
    std::vector<core::VimRowSpan> spans;
    // 表示領域は engine が求めたときだけ渡す（ADR 0129 の決定 2）。追従を解決するのはその時だけ。
    core::VimEditorView view{
        state_.text(), state_.selection(), std::nullopt,
        replay_depth_.has_value() ? core::VimKeySource::replayed : core::VimKeySource::typed,
        core::VimTabs{state_.active_tab(), state_.tab_count()},
        // 格子は常に折り返す（ADR 0140 の決定 7）。wrap off の text 形は無効のまま（gj gk は j k・
        // ADR 0145 の決定 4）。横送りの row_request は有効か否かに関わらず答える。
        // 格子の画面移動は画面行の単位（ADR 0144 の決定 5）。text の形は wrap でも論理行のまま。
        // 1 画面行の升の数は文字組みの問いに答えるときに一緒に置く（決定 10）。
        core::VimLayoutView{view_geometry_.width > 0 &&
                                (state_.settings().wrapping == core::LineWrapping::wrap ||
                                 state_.display() == core::DisplayForm::grid),
                            rows,
                            state_.display() == core::DisplayForm::grid ? core::VimScrollUnit::row
                                                                        : core::VimScrollUnit::line,
                            spans}};
    // 文書のマークを貸す（ADR 0152 の決定 2）。この関数の中では状態を変えないので参照は保たれる。
    view.marks = &state_.vim_marks();
    auto step = engine_step(state_.vim(), view, source);
    // どの要求も同じ 1 歩（鍵か行番号）をもう一度流す。viewport は 1 度渡したら以後も渡したまま。
    // 回数の上限は持たない。同じ問いの立て直しと viewport の二重要求は inconsistent で止まり、
    // 新しい答えは行数 + 画面行の総数 + 1 で尽きる（ADR 0129・ADR 0133 の追補 2026-10-18）。
    while (step.viewport_request || step.layout_request.has_value() || step.row_request.has_value())
    {
        const auto answered = answered_vim_request(step, view, rows, spans);
        if (!answered)
        {
            return std::unexpected(answered.error());
        }
        step = engine_step(state_.vim(), view, source);
    }
    return step;
}

std::expected<void, TextLayoutFailure>
EditorController::answered_vim_request(const core::VimStep &step, core::VimEditorView &view,
                                       std::vector<core::VimLineRows> &rows,
                                       std::vector<core::VimRowSpan> &spans)
{
    if (step.layout_request.has_value() || step.row_request.has_value())
    {
        // 1 画面行の升の数は文字組みの問いに答える所で 1 回だけ置く（ADR 0144 の決定 10）。
        auto text = visual_view();
        view.layout.row_columns = text.row_columns();
        if (step.layout_request.has_value())
        {
            const auto measured = measured_rows(rows, step.layout_request.value(), text);
            view.layout.lines = rows;
            return measured;
        }
        const auto measured = measured_span(spans, step.row_request.value(), text);
        view.layout.spans = spans;
        return measured;
    }
    const auto unasked = vim_viewport_unasked(view.viewport);
    if (!unasked)
    {
        return unasked;
    }
    const auto scroll = settled_scroll();
    // 画面の幅（升）と今の横位置（leftcol）も同じ往復で答える（ADR 0145 の決定 2）。横に送らない
    // 文書では幅 0 で、文字組みを呼ばない。
    auto text = visual_view();
    const auto columns = text.horizontal_columns();
    const auto left =
        columns == 0
            ? std::expected<std::size_t, TextLayoutFailure>{0}
            : text.column_of_horizontal(state_.text().position_of(state_.selection().caret).line,
                                        scroll.horizontal);
    if (!left)
    {
        return std::unexpected(left.error());
    }
    view.viewport = core::VimViewport{scroll.first_visible, scroll.visible_lines, scroll.first_row,
                                      columns, left.value()};
    return {};
}
} // namespace nenenib::application
