// 補完の面（ADR 0190 の決定 1・2・5・ADR 0169 の決定 3〜5）。開く・絞る・選ぶ・確定・閉じるの群。
// 候補を並べるのと絞るのは core::completion_entries / completion_matches、本文の語は
// insert_completion_list（席 W の入口）、LSP の要求と答えは段 A の request_completion と
// language_answers の 1 本ずつで、ここは CompletionSession を動かすだけ。面が開いている間の打鍵は
// 本文へ通し（Ctrl+P の面とは違う）、意図の出口（settle_completion）が絞り直す。
#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionCandidates.hpp"
#include "CompletionDirection.hpp"
#include "CompletionEntries.hpp"
#include "CompletionEntry.hpp"
#include "CompletionFace.hpp"
#include "CompletionRowView.hpp"
#include "CompletionSession.hpp"
#include "CompletionSource.hpp"
#include "CompletionStep.hpp"
#include "CompletionView.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "EditBoundary.hpp"
#include "EditMode.hpp"
#include "EditorControllerDetail.hpp"
#include "InsertText.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageCompletion.hpp"
#include "LanguageServerPhase.hpp"
#include "LineEnding.hpp"
#include "LspCompletionList.hpp"
#include "LspDiagnostics.hpp"
#include "LspRequestKind.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenCompletion.hpp"
#include "ScrollCompletion.hpp"
#include "StepCompletion.hpp"
#include "UiMessage.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
// 語の頭からキャレットまでのバイト数の上限。これを越えて打ったら面を閉じる（絞り直しのたびに
// 写す量を行の長さに比例させない）。
constexpr std::size_t typed_limit = 1024;

[[nodiscard]] bool is_completion_intent(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<OpenCompletion>(intent) ||
           std::holds_alternative<StepCompletion>(intent) ||
           std::holds_alternative<ChooseCompletion>(intent) ||
           std::holds_alternative<CloseCompletion>(intent) ||
           std::holds_alternative<ScrollCompletion>(intent);
}

// 面を開いたまま受ける意図（ADR 0190 の決定 5）。面の意図・文字の入力・手前への削除（絞り直す）と、
// 使う人の操作でない意図（合図・窓の寸法・帯の hover）。ほかの使う人の操作（キャレットの移動・
// クリック・ホイール・タブの切り替え・undo・変換・モードの切り替え…）は写す前に面を閉じる。
[[nodiscard]] bool keeps_completion(const EditorIntent &intent) noexcept
{
    const auto *deleted = std::get_if<DeleteText>(&intent);
    return is_completion_intent(intent) || std::holds_alternative<InsertText>(intent) ||
           (deleted != nullptr && deleted->direction == core::DeleteDirection::backward) ||
           detail::not_user_operation(intent);
}

// 面に出す行があるか。自動で開いた面（triggerCharacters）は LSP の候補が当たっているときだけ出す
// （本文の語だけで `.` のたびに面を出さない）。LSP の候補は列の先頭にある。
[[nodiscard]] bool has_rows(const CompletionSession &session) noexcept
{
    if (session.shown.empty())
    {
        return false;
    }
    return session.by_hand || session.entries.at(session.shown.front()).source ==
                                  core::CompletionSource::language_server;
}

// 面を持ち続けるか。出す行があるか、LSP の答えを待っている（届いて候補があれば出す）。
[[nodiscard]] bool alive(const CompletionSession &session) noexcept
{
    return has_rows(session) || session.awaiting;
}

// 選んでいる行が見えるように送った窓の先頭。
[[nodiscard]] std::size_t window_first(std::size_t first, std::size_t selected) noexcept
{
    if (selected < first)
    {
        return selected;
    }
    return selected >= first + core::completion_face_rows
               ? selected + 1 - core::completion_face_rows
               : first;
}

// 上下は回り込み、PageUp / PageDown は面の行数ぶんで端に止まる（Ctrl+P の面と同じ）。count は
// 1 以上。
[[nodiscard]] std::size_t stepped_row(std::size_t selected, std::size_t count,
                                      CompletionStep step) noexcept
{
    switch (step)
    {
    case CompletionStep::previous:
        return (selected + count - 1) % count;
    case CompletionStep::next:
        return (selected + 1) % count;
    case CompletionStep::page_previous:
        return selected - std::min(selected, core::completion_face_rows);
    case CompletionStep::page_next:
        return std::min(selected + core::completion_face_rows, count - 1);
    }
    std::unreachable();
}

// 持っている候補の列を打った分 typed で絞る（本文を走らない）。選んでいる行と窓は先頭へ戻し、
// 「使う人が選ぶ行を動かした」の印も下ろす（絞った後に届いた答えは先頭を選ぶ・#1844 の R5）。
void narrow(CompletionSession &session, std::string_view typed)
{
    session.shown = core::completion_matches(session.entries, typed);
    session.selected = 0;
    session.first = 0;
    session.stepped = false;
    session.detailed =
        std::ranges::any_of(session.shown, [&session](std::size_t position)
                            { return !session.entries.at(position).detail.empty(); });
}

// LSP の候補か本文の語が変わったときに候補の列を作り直し、打った分 typed で絞る。
void arrange(CompletionSession &session, std::string_view typed)
{
    session.entries = core::completion_entries(session.server, session.words);
    narrow(session, typed);
}

// 名前と出どころが same と同じ候補を選び直し、動かした印を立て直す（無ければ先頭のまま）。
void reselect(CompletionSession &session, const core::CompletionEntry &same)
{
    const auto found =
        std::ranges::find_if(session.shown,
                             [&session, &same](std::size_t position)
                             {
                                 const auto &entry = session.entries.at(position);
                                 return entry.label == same.label && entry.source == same.source;
                             });
    if (found != session.shown.end())
    {
        session.selected = static_cast<std::size_t>(found - session.shown.begin());
        session.first = window_first(0, session.selected);
        session.stepped = true;
    }
}

// lines 行送った窓の先頭（正は下）。候補が面の行数以下なら 0 のまま。
[[nodiscard]] std::size_t scrolled_first(std::size_t first, std::size_t count,
                                         std::int32_t lines) noexcept
{
    const std::size_t last = count - std::min(count, core::completion_face_rows);
    const auto moved = static_cast<std::int64_t>(first) + lines;
    return static_cast<std::size_t>(
        std::clamp<std::int64_t>(moved, 0, static_cast<std::int64_t>(last)));
}

// 確定で入れる平文。LSP の本文の改行（LF か CRLF）は文書の改行の形にする（ARC-009）。CR は
// 1 バイトずつ落とす（std::erase は MSVC STL の __std_remove_1 を application
// の外へ出す・ARC-003）。
[[nodiscard]] std::string inserted_text(std::string_view text, std::string_view newline)
{
    std::string plain;
    plain.reserve(text.size());
    for (const char byte : text)
    {
        if (byte != '\r')
        {
            plain.push_back(byte);
        }
    }
    return detail::with_document_newlines(plain, newline);
}
} // namespace

bool EditorController::completion_active() const noexcept
{
    return completion_.has_value() && has_rows(completion_.value());
}

bool EditorController::completion_open() const noexcept
{
    return completion_.has_value();
}

void EditorController::close_completion_before(const EditorIntent &intent)
{
    if (completion_.has_value() && !keeps_completion(intent))
    {
        close_completion();
    }
}

// 面を閉じる 1 か所。表に残っている補完の要求を取り消し、届いていた補完の答えを状態から消す
// （ADR 0190 の決定 1）。取り消しは awaiting に依らない（出し直しを送れなかった後も前の要求が表に
// 残り得るので、遅れた答えが面の無い状態に置かれる筋を閉じる・#1844 の R6）。
void EditorController::close_completion()
{
    if (!completion_.has_value())
    {
        return;
    }
    cancel_lsp_requests(LspRequestKind::completion);
    completion_.reset();
    forget_completion_answer();
}

void EditorController::forget_completion_answer()
{
    if (!state_.language_answers().completion.has_value())
    {
        return;
    }
    LanguageAnswers answers = state_.language_answers();
    answers.completion.reset();
    state_ = state_.with_language_answers(std::move(answers));
}

// 面を開ける状態か。通常モード（Vim の INSERT の道は段 B2）・入力行と面とエクスプローラーの焦点と
// 全面プレビューの外・変換中でない（ADR 0014）・選択が無い。
bool EditorController::completion_startable() const
{
    switch (state_.mode())
    {
    case core::EditMode::vim:
        return false;
    case core::EditMode::ordinary:
        break;
    }
    const auto &selection = state_.selection();
    return !state_.command_input().has_value() && !state_.composition().has_value() &&
           !explorer_focused() && state_.full_preview() == nullptr &&
           selection.anchor == selection.caret;
}

// 開いている面がまだ成り立つか。同じ文書がアクティブで、キャレットが語の頭より前へ行かず、語の
// 頭からキャレットまでが 1 つの接頭辞のまま（語の文字でない文字を打ったら偽）。
bool EditorController::completion_holds(const CompletionSession &session) const
{
    const core::Offset caret = state_.selection().caret;
    if (!completion_startable() ||
        state_.autosave_ids().at(state_.active_tab()) != session.document ||
        caret < session.origin || caret.value - session.origin.value > typed_limit)
    {
        return false;
    }
    return core::completion_prefix_kept(state_.text().text_range(session.origin, caret));
}

void EditorController::accept(const OpenCompletion &)
{
    close_completion();
    start_completion(std::nullopt);
}

// 通常モードで `triggerCharacters` の 1 つを打ち終えたら開く（決定 5）。呼ぶのは言語サーバーが
// running のときだけ（打鍵の道の分岐はその真偽の後ろ・決定 10）。typed は今打った文字。
void EditorController::open_triggered_completion(std::string_view typed)
{
    if (typed.empty() || !completion_startable())
    {
        return;
    }
    const core::Offset caret = state_.selection().caret;
    for (const std::string &trigger : language_server_.abilities.trigger_characters)
    {
        if (trigger.empty() || !trigger.ends_with(typed) || caret.value < trigger.size())
        {
            continue;
        }
        const core::Offset from{caret.value - trigger.size()};
        if (state_.text().text_range(from, caret) == trigger)
        {
            close_completion();
            start_completion(trigger);
            return;
        }
    }
}

// キャレットの直前の語の頭（席 W の接頭辞の決め方 completion_prefix_start の 1 本）。キャレットの
// 手前の窓（typed_limit バイト）だけを読み、本文の語は集めない。
core::Offset EditorController::completion_word_start() const
{
    const core::Offset caret = state_.selection().caret;
    const std::size_t from = caret.value - std::min(caret.value, typed_limit);
    const std::string window = state_.text().text_range(core::Offset{from}, caret);
    const std::size_t skipped = from > 0 ? core::leading_continuations(window) : 0;
    const std::string_view content = std::string_view{window}.substr(skipped);
    return core::Offset{from + skipped + core::completion_prefix_start(content, content.size())};
}

// 面を開く 1 か所。trigger が無ければ手で開いた面（LSP の候補 ＋ 本文の語・語をすぐ出す）、あれば
// 自動の面（LSP の候補だけ・本文の語を集めず語の表も作らない・#1844 の R3 / R4 の決め）。語の頭は
// どちらも席 W の接頭辞の始まり（trigger が語の文字のサーバーでも、確定は語の頭から置き換える・
// M1）。出す行も待つ答えも無ければ開かず、手で開いたときだけ 1 行知らせる。
void EditorController::start_completion(std::optional<std::string_view> trigger)
{
    if (!completion_startable())
    {
        return;
    }
    const bool by_hand = !trigger.has_value();
    const core::Offset caret = state_.selection().caret;
    CompletionSession session{.id = ++completion_sessions_,
                              .document = state_.autosave_ids().at(state_.active_tab()),
                              .origin = completion_word_start(),
                              .by_hand = by_hand,
                              .caret = caret,
                              .revision = state_.history().revision()};
    if (by_hand)
    {
        core::CompletionList list = insert_completion(core::CompletionDirection::forward);
        session.origin = list.prefix.begin;
        session.words = std::move(list.candidates);
        session.words_collected = true;
    }
    session.awaiting = request_completion(session.id, trigger);
    arrange(session, state_.text().text_range(session.origin, caret));
    if (alive(session))
    {
        completion_ = std::move(session);
    }
    else if (by_hand)
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_completion_none));
    }
}

// 意図の出口の 1 か所（apply）。面が成り立たなくなっていれば閉じ、届いた答えを列に足し、本文か
// キャレットが動いていれば絞り直す。出す行も待つ答えも無くなったら閉じる（手で開いた面が答えを
// 待っていて、届いた答えにも候補が無かったときだけ 1 行知らせる）。面が無ければ欄を 1 つ読むだけで
// 抜ける。
void EditorController::settle_completion()
{
    if (!completion_.has_value())
    {
        return;
    }
    CompletionSession &session = completion_.value();
    if (!completion_holds(session))
    {
        close_completion();
        return;
    }
    const bool answered = take_completion_answer(session);
    // 言語サーバーが止まったら、もう答えは届かない（待っているだけの面はここで閉じる）。
    if (language_server_.phase != LanguageServerPhase::running)
    {
        session.awaiting = false;
    }
    if (session.caret != state_.selection().caret ||
        session.revision != state_.history().revision())
    {
        refresh_completion(session);
    }
    if (alive(session))
    {
        return;
    }
    const bool unanswered = answered && session.by_hand;
    close_completion();
    if (unanswered)
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_completion_none));
    }
}

// 打った後の絞り直し（決定 1・5）。持っている列を接頭辞で絞るだけで、本文を走らず、列も作り
// 直さない。最後の答えが `isIncomplete` のときだけ、古い要求を取り消して LSP の要求を出し直す
// （本文の語は面を開いたときの列を使い回す・届くまでは手元の候補を絞って見せる・#1844 の R2）。
void EditorController::refresh_completion(CompletionSession &session)
{
    const core::Offset caret = state_.selection().caret;
    session.caret = caret;
    session.revision = state_.history().revision();
    narrow(session, state_.text().text_range(session.origin, caret));
    if (session.incomplete)
    {
        session.awaiting = request_completion(session.id);
        session.incomplete = session.awaiting;
    }
}

// 届いた補完の答え（段 A が状態に置いた 1 つ）を状態から消し、面の通し番号と文書が合えば面の列に
// 足す（決定 1・5）。足したら真。面が無いときの答えには触れない（面を閉じるときに消してある）。
bool EditorController::take_completion_answer(CompletionSession &session)
{
    const auto &held = state_.language_answers().completion;
    if (!held.has_value())
    {
        return false;
    }
    const LanguageCompletion answer = held.value();
    forget_completion_answer();
    if (answer.session != session.id || answer.document != session.document)
    {
        return false;
    }
    merge_completion_answer(session, answer.list);
    return true;
}

// LSP の候補を入れ替えて並べ直す。使う人が選ぶ行を動かしていれば選んでいる候補を保ち、動かして
// いなければ先頭（LSP の最初の候補）を選ぶ。本文の語は面を開いたときの列のまま（自動の面は空）。
void EditorController::merge_completion_answer(CompletionSession &session,
                                               const core::LspCompletionList &list) const
{
    const std::string typed = state_.text().text_range(session.origin, state_.selection().caret);
    const std::optional<core::CompletionEntry> chosen =
        session.stepped && has_rows(session)
            ? std::optional{session.entries.at(session.shown.at(session.selected))}
            : std::nullopt;
    session.server = core::completion_entries_of(list);
    session.incomplete = list.incomplete;
    session.awaiting = false;
    arrange(session, typed);
    if (chosen.has_value())
    {
        reselect(session, chosen.value());
    }
}

void EditorController::accept(const StepCompletion &intent)
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return;
    }
    auto &session = completion_.value();
    session.selected = stepped_row(session.selected, session.shown.size(), intent.step);
    session.first = window_first(session.first, session.selected);
    session.stepped = true;
}

// 面のホイール（#1844 の R1）。窓だけを送り、選んでいる行は見えなくなったら窓の端へ寄せる。
void EditorController::accept(const ScrollCompletion &intent)
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return;
    }
    auto &session = completion_.value();
    session.first = scrolled_first(session.first, session.shown.size(), intent.lines);
    const std::size_t last =
        std::min(session.first + core::completion_face_rows, session.shown.size()) - 1;
    session.selected = std::clamp(session.selected, session.first, last);
}

// row 番目の候補を確定したときの編集（置き換える範囲と入れる平文）。
std::pair<core::OffsetRange, std::string>
EditorController::completion_edit(const CompletionSession &session, std::size_t row) const
{
    const core::CompletionEntry &entry = session.entries.at(session.shown.at(row));
    return {completion_range(session.origin, entry),
            inserted_text(entry.insert, core::newline_of(state_.line_ending()))};
}

// 選んでいる候補の確定が本文を変えるか（面が見えていなければ偽）。窓が Enter と Tab を面へ送るか
// を決める安い読みで、変えないなら窓は面を閉じて鍵を本文へ通す（打った語と同じ綴りだけが残って
// いるときに Enter が空振りしない・#1844 の L1）。
bool EditorController::completion_commits() const
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return false;
    }
    const auto [range, text] = completion_edit(completion_.value(), completion_.value().selected);
    return state_.text().text_range(range.begin, range.end) != text;
}

// 確定（決定 5）。本文に入れるのは replace の 1 か所の 1 編集（undo の 1 単位）で、面は先に閉じる
// （出口の絞り直しに掛けない）。入れる平文が今の本文と同じなら編集を積まない。row があればその
// 候補（面の行のクリック）、無ければ選んでいる候補。
void EditorController::accept(const ChooseCompletion &intent)
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return;
    }
    const auto &session = completion_.value();
    const std::size_t row = intent.row.value_or(session.selected);
    if (row >= session.shown.size())
    {
        return;
    }
    const auto [range, text] = completion_edit(session, row);
    close_completion();
    if (state_.text().text_range(range.begin, range.end) != text)
    {
        replace(range, text, core::EditBoundary::separate);
    }
}

void EditorController::accept(const CloseCompletion &)
{
    close_completion();
}

// 確定で置き換える範囲（決定 5）。`textEdit` の範囲がキャレットの行の中に在って始まりがキャレット
// より前ならその始まりから、そうでなければ語の頭から、キャレットまで。範囲は要求を出した版の位置
// だが、面が開いている間の編集は語の頭より後ろだけなので、始まりは今の本文でも同じ所を指す。
// 終わりはキャレット（`InsertReplaceEdit` の insert の形・打ち足した分も置き換える）。
core::OffsetRange EditorController::completion_range(core::Offset origin,
                                                     const core::CompletionEntry &entry) const
{
    const core::Offset caret = state_.selection().caret;
    const core::OffsetRange prefix{origin, caret};
    if (!entry.range.has_value())
    {
        return prefix;
    }
    const auto placed =
        core::lsp_range_to_offsets(state_.text(), entry.range.value(), language_server_.encoding);
    const auto line_of = [this](core::Offset at)
    { return state_.text().position_of(at).line.value; };
    if (!placed.has_value() || caret < placed.value().begin ||
        line_of(placed.value().begin) != line_of(caret) ||
        line_of(placed.value().end) != line_of(caret))
    {
        return prefix;
    }
    return core::OffsetRange{placed.value().begin, caret};
}

std::optional<CompletionView> EditorController::completion_view() const
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return std::nullopt;
    }
    const auto &session = completion_.value();
    CompletionView view{.session = session.id,
                        .rows = {},
                        .selected = session.selected - session.first,
                        .first = session.first,
                        .total = session.shown.size(),
                        .detailed = session.detailed};
    const std::size_t count =
        std::min(core::completion_face_rows, session.shown.size() - session.first);
    view.rows.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto &entry = session.entries.at(session.shown.at(session.first + index));
        view.rows.push_back(CompletionRowView{entry.label, entry.detail, entry.source});
    }
    return view;
}
} // namespace nenenib::application
