// 補完の面（ADR 0190 の決定 1・2・5・ADR 0169 の決定 3〜5）。開く・絞る・選ぶ・確定・閉じるの群。
// Vim の INSERT の補完（#1845）は engine が形を持ち、ここは候補の写しを engine に置く所
// （offer_vim_completion）と、engine の状態を面に写す所（follow_vim_completion）だけを持つ。
// Vim の INSERT で打鍵が自動に開く面（#1962・ADR 0190 の 2026-11-23 の追補）は engine の外で、
// 通常モードの自動の面と同じ道で絞り、Ctrl-n / Ctrl-p から engine の補完へ移る。
// 候補を並べるのと絞るのは core::completion_entries / completion_matches、本文の語は
// insert_completion_list（席 W の入口）、LSP の要求と答えは段 A の request_completion と
// language_answers の 1 本ずつで、ここは CompletionSession を動かすだけ。面が開いている間の打鍵は
// 本文へ通し（Ctrl+P の面とは違う）、意図の出口（settle_completion）が絞り直す。
#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionCandidates.hpp"
#include "CompletionChoice.hpp"
#include "CompletionCommitKey.hpp"
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
#include "LanguageServerSlot.hpp"
#include "LineEnding.hpp"
#include "LspCompletionList.hpp"
#include "LspDiagnostics.hpp"
#include "LspPosition.hpp"
#include "LspRange.hpp"
#include "LspRequestKind.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenCompletion.hpp"
#include "ScopedValue.hpp"
#include "ScrollCompletion.hpp"
#include "StepCompletion.hpp"
#include "UiMessage.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimCompletionOffer.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimKeySource.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimStepSource.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
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
// Vim のモードの面（#1845）は engine が持つので、開いたまま受けるのは Vim の鍵（何で閉じるかは
// engine が決める）・面の行のクリックとホイール・使う人の操作でない意図だけ。
[[nodiscard]] bool keeps_completion(const EditorIntent &intent, core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::vim:
        return std::holds_alternative<VimKeyPress>(intent) ||
               std::holds_alternative<ChooseCompletion>(intent) ||
               std::holds_alternative<StepCompletion>(intent) ||
               std::holds_alternative<ScrollCompletion>(intent) ||
               detail::not_user_operation(intent);
    case core::EditMode::ordinary:
        break;
    }
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

// 自動で開いた面（engine の外）で、使う人がまだ選ぶ行を動かしていないか（#1971）。面はどの行も
// 選ばずに出し、Tab はいちばん上の候補で確定し、Enter は確定しない。選んでいる位置 selected は
// 先頭のまま（Tab と行の名指しの無い確定が読む）。
[[nodiscard]] bool awaits_choice(const CompletionSession &session) noexcept
{
    return !session.by_hand && !session.stepped;
}

// まだ選んでいない面の最初の 1 歩（先頭の前から動く）。下と PageDown は先頭の側から、上は最後の行、
// PageUp は先頭で止まる。count は 1 以上。
[[nodiscard]] std::size_t first_choice(std::size_t count, CompletionStep step) noexcept
{
    switch (step)
    {
    case CompletionStep::previous:
        return count - 1;
    case CompletionStep::next:
    case CompletionStep::page_previous:
        return 0;
    case CompletionStep::page_next:
        return std::min(core::completion_face_rows, count) - 1;
    }
    std::unreachable();
}

// engine の補完の中の 1 つの面の歩が、Ctrl-n / Ctrl-p の何歩か（#2024）。selected が size 以上なら
// 元の接頭辞（輪の端）。PageDown / PageUp は面の行数ぶんで列の端で止まる。
[[nodiscard]] std::size_t engine_steps(CompletionStep step, std::size_t selected,
                                       std::size_t size) noexcept
{
    const bool original = selected >= size;
    switch (step)
    {
    case CompletionStep::previous:
    case CompletionStep::next:
        return 1;
    case CompletionStep::page_next:
        return std::min(core::completion_face_rows, original ? size : size - 1 - selected);
    case CompletionStep::page_previous:
        return original ? 0 : std::min(core::completion_face_rows, selected);
    }
    std::unreachable();
}

[[nodiscard]] bool is_tab(const core::VimKey &key) noexcept
{
    const auto *character = std::get_if<core::VimCharacter>(&key);
    return character != nullptr && character->code == U'\t';
}

[[nodiscard]] bool is_special(const core::VimKey &key, core::VimSpecialKey wanted) noexcept
{
    const auto *special = std::get_if<core::VimSpecialKey>(&key);
    return special != nullptr && *special == wanted;
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
    if (completion_.has_value() && !keeps_completion(intent, state_.mode()))
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
    // 補完の要求はどの席に残っていても取り消す（席は文書の言語ごと・段 S2）。
    for (const LanguageServerSlot slot : language_server_slots)
    {
        cancel_lsp_requests(slot, LspRequestKind::completion);
    }
    // 打鍵で自動に開いた面から移った engine の補完は、補完の鍵をマクロに積んでいないので、確定する
    // 語を打った本文としてここで積む（#1971・D109）。
    if (triggered_in_engine())
    {
        record_triggered_typed(std::nullopt);
    }
    completion_.reset();
    forget_completion_answer();
    // Vim の INSERT の補完は engine の側も閉じる（#1845・選んでいた語は確定・記録には打った本文と
    // して足す）。開いていなければ何も変えない。
    if (state_.vim().insert_completion != nullptr || state_.vim().completion_offer != nullptr)
    {
        state_ = state_.with_vim(core::vim_completion_settled(state_.vim()));
    }
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

// 面を開ける状態か。通常モードか、Vim の INSERT で打鍵が自動に開く面（triggered・engine の
// 補完が閉じている間・#1962。Vim の手で開く面は engine の Ctrl-n の道 offer_vim_completion）。
// どちらも入力行と面とエクスプローラーの焦点と全面プレビューの外・変換中でない（ADR 0014）・
// 選択が無い。
bool EditorController::completion_startable(bool triggered) const
{
    switch (state_.mode())
    {
    case core::EditMode::vim:
        if (!triggered || state_.vim().mode != core::VimMode::insert ||
            state_.vim().insert_completion != nullptr)
        {
            return false;
        }
        break;
    case core::EditMode::ordinary:
        break;
    }
    const auto &selection = state_.selection();
    // 設定の面が開いている間と、この意図の入口で設定の面を閉じた直後は開かない（面どうしの排他）。
    return !settings_open() && !settings_closed_now_ && !state_.command_input().has_value() &&
           !state_.composition().has_value() && !explorer_focused() &&
           state_.full_preview() == nullptr && selection.anchor == selection.caret;
}

// 開いている面がまだ成り立つか。同じ文書がアクティブで、キャレットが語の頭より前へ行かず、語の
// 頭からキャレットまでが 1 つの接頭辞のまま（語の文字でない文字を打ったら偽）。
bool EditorController::completion_holds(const CompletionSession &session) const
{
    const core::Offset caret = state_.selection().caret;
    if (!completion_startable(!session.by_hand) ||
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

// 打った文字の後の言語サーバーの入口（#1962）。補完の自動の面と、引数の情報（#1959・ADR 0191 の
// 決定 6）の 2 つ。通常モードの accept(InsertText) と Vim の INSERT の打った文字の両方が
// ここへ来る。
void EditorController::typed_for_language(std::string_view typed)
{
    if (active_language_server().phase == LanguageServerPhase::running)
    {
        open_triggered_completion(typed);
        // 引数の情報は補完と並べてこの枝の後ろ（open_triggered_completion の中には足さない）。
        settle_typed_signature(typed);
    }
}

// `triggerCharacters` の 1 つを打ち終えたら開く（決定 5・Vim の INSERT は #1962 の追補）。呼ぶのは
// 言語サーバーが running のときだけ（打鍵の道の分岐はその真偽の後ろ・決定 10）。typed は今打った
// 文字。
void EditorController::open_triggered_completion(std::string_view typed)
{
    if (typed.empty() || !completion_startable(true))
    {
        return;
    }
    const core::Offset caret = state_.selection().caret;
    for (const std::string &trigger : active_language_server().abilities.trigger_characters)
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
    if (!completion_startable(trigger.has_value()))
    {
        return;
    }
    // 補完の面と hover の札は同時に出さない（ADR 0190 の決定 5・6）。面を開くときに札を消す。
    drop_hover();
    const bool by_hand = !trigger.has_value();
    const core::Offset caret = state_.selection().caret;
    CompletionSession session{.id = ++completion_sessions_,
                              .document = state_.autosave_ids().at(state_.active_tab()),
                              .origin = completion_word_start(),
                              .by_hand = by_hand,
                              .caret = caret,
                              .revision = state_.history().revision()};
    session.triggered = !by_hand;
    if (by_hand)
    {
        core::CompletionList list = insert_completion(core::CompletionDirection::forward);
        session.origin = list.prefix.begin;
        session.words = std::move(list.candidates);
        session.words_collected = true;
    }
    session.awaiting = request_completion(session.id, trigger);
    std::string typed = state_.text().text_range(session.origin, caret);
    arrange(session, typed);
    session.requested = std::move(typed);
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
    // Vim の手で開いた面は engine が持つ。打鍵で自動に開いた面（#1962）は engine の外で、通常
    // モードと同じに絞り直す。
    if (state_.mode() == core::EditMode::vim && session.by_hand)
    {
        settle_vim_completion(session);
        return;
    }
    if (!completion_holds(session))
    {
        close_completion();
        return;
    }
    const bool answered = take_completion_answer(session);
    // 言語サーバーが止まったら、もう答えは届かない（待っているだけの面はここで閉じる）。
    if (active_language_server().phase != LanguageServerPhase::running)
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
    std::string typed = state_.text().text_range(session.origin, caret);
    narrow(session, typed);
    if (session.incomplete)
    {
        session.awaiting = request_completion(session.id);
        session.incomplete = session.awaiting;
        session.requested = std::move(typed);
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
    // Vim の手で開いた面（#1845）は engine の列に足す。
    if (state_.mode() == core::EditMode::vim && session.by_hand)
    {
        merge_vim_completion_answer(session, answer.list);
        return true;
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
    // Vim の engine の補完は engine が選ぶ（#2024）。engine の外の自動の面は通常モードと同じに選ぶ
    // だけ（本文を変えない・録画に積まない）。
    if (state_.mode() == core::EditMode::vim && state_.vim().insert_completion != nullptr)
    {
        step_engine_completion(intent.step);
        return;
    }
    auto &session = completion_.value();
    session.selected = awaits_choice(session)
                           ? first_choice(session.shown.size(), intent.step)
                           : stepped_row(session.selected, session.shown.size(), intent.step);
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
    // Vim の面の選んでいる行は engine が正（本文に入っている語）なので、窓だけを送る（#1845）。まだ
    // 選んでいない自動の面も窓だけを送り、選ばないまま（#1971）。
    if (state_.mode() == core::EditMode::vim || awaits_choice(session))
    {
        return;
    }
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

// key で確定する候補の確定が本文を変えるか（面が見えていなければ偽）。窓が Enter と Tab を面へ
// 送るかを決める安い読みで、変えないなら窓は面を閉じて鍵を本文へ通す（打った語と同じ綴りだけが
// 残っているときに Enter が空振りしない・#1844 の L1）。自動で開いてまだ選んでいない面の Enter は
// 確定しない（改行を通す）、Tab は先頭の候補で確定する（#1971）。
bool EditorController::completion_commits(CompletionCommitKey key) const
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return false;
    }
    switch (key)
    {
    case CompletionCommitKey::enter:
        if (awaits_choice(completion_.value()))
        {
            return false;
        }
        break;
    case CompletionCommitKey::tab:
        break;
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
    if (state_.mode() == core::EditMode::vim)
    {
        choose_vim_completion(intent.row);
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
    const auto placed = core::lsp_range_to_offsets(state_.text(), entry.range.value(),
                                                   active_language_server().encoding);
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

// ---------------------------------------------------------------- Vim の INSERT の補完（#1845）

namespace
{
// engine に置く候補の写し。面の行（shown の順）と同じ順で、どの語も語の頭からキャレットまでと
// 置き換える本文。
[[nodiscard]] std::shared_ptr<const std::vector<std::string>>
vim_words(const CompletionSession &session)
{
    auto words = std::make_shared<std::vector<std::string>>();
    words->reserve(session.shown.size());
    for (const std::size_t position : session.shown)
    {
        words->push_back(session.entries.at(position).insert);
    }
    return words;
}

// Vim の面の候補の列。LSP の候補が先、本文の語が後で、入れる本文が LSP の候補と同じ語は落とす
// （選んでいる語の綴りは必ず列に残る）。全部の行を出す（絞るのは engine の外で済んでいる）。
void arrange_vim(CompletionSession &session)
{
    std::vector<core::CompletionCandidate> words;
    for (const core::CompletionCandidate &word : session.words)
    {
        if (std::ranges::none_of(session.server, [&word](const core::CompletionEntry &entry)
                                 { return entry.insert == word.word; }))
        {
            words.push_back(word);
        }
    }
    session.entries = session.server;
    for (core::CompletionEntry &entry : core::completion_entries({}, words))
    {
        session.entries.push_back(std::move(entry));
    }
    session.shown.resize(session.entries.size());
    for (std::size_t position = 0; position < session.shown.size(); ++position)
    {
        session.shown.at(position) = position;
    }
    session.detailed = std::ranges::any_of(session.entries, [](const core::CompletionEntry &entry)
                                           { return !entry.detail.empty(); });
}
} // namespace

// 補完を始める鍵（閉じているときの Ctrl-n / Ctrl-p・engine の vim_starts_completion が決める）の
// 直前に、候補の列を鍵の向きで作って engine に置く（ADR 0169 の決定 3・ADR 0051 の `"+` の写しと
// 同じ形）。本文の語は席 W の insert_completion の順で、LSP の候補は答えが届いてから列の先頭側に
// 足す（決定 5）。変換中は始めない（ADR 0014）。出す語も待つ答えも無ければ写しを置かず、engine は
// 何もしない（Vim の「Pattern not found」）。
void EditorController::offer_vim_completion(const core::VimKey &key)
{
    const auto direction = core::vim_starts_completion(state_.vim(), key);
    // 設定の面が開いている間と、この意図の入口で設定の面を閉じた直後は始めない（面どうしの排他・
    // completion_startable と同じ読み）。
    if (!direction.has_value() || state_.composition().has_value() || settings_open() ||
        settings_closed_now_)
    {
        return;
    }
    // 打鍵で自動に開いた面が出ていれば、その候補から選ぶ（#1962）。答えを待っているだけの自動の
    // 面は取り消して本文の語の補完を開く（下の道）。
    if (completion_.has_value() && !completion_.value().by_hand && has_rows(completion_.value()) &&
        offer_auto_vim_completion())
    {
        return;
    }
    close_completion();
    // 補完の面と hover の札は同時に出さない（ADR 0190 の決定 5・6）。
    drop_hover();
    const core::Offset caret = state_.selection().caret;
    core::CompletionList list = insert_completion(direction.value());
    CompletionSession session{.id = ++completion_sessions_,
                              .document = state_.autosave_ids().at(state_.active_tab()),
                              .origin = list.prefix.begin,
                              .by_hand = true,
                              .caret = caret,
                              .revision = state_.history().revision()};
    session.words = std::move(list.candidates);
    session.words_collected = true;
    session.awaiting = request_completion(session.id, std::nullopt);
    arrange_vim(session);
    if (!alive(session))
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_completion_none));
        return;
    }
    state_ = state_.with_vim(core::vim_completion_offered(
        state_.vim(), core::VimCompletionOffer{vim_words(session), session.origin}));
    completion_ = std::move(session);
}

// Vim の鍵の後に、engine の補完の状態を面に写す（選んでいる行は engine が正）。engine の補完が
// 閉じていれば面も閉じる。通常モードの面には触れない（Vim の鍵の道は Vim のモードでだけ通る）。
void EditorController::follow_vim_completion()
{
    const auto &engine = state_.vim().insert_completion;
    if (engine == nullptr || !completion_.has_value())
    {
        // 打鍵で自動に開いた面（#1962）は engine の外なので、鍵の後も閉じない（成り立つかは
        // 出口の settle_completion が決める）。
        if (engine == nullptr && completion_.has_value() && !completion_.value().by_hand)
        {
            return;
        }
        close_completion();
        return;
    }
    CompletionSession &session = completion_.value();
    session.selected = engine->selected;
    if (session.selected < session.shown.size())
    {
        session.first = window_first(session.first, session.selected);
    }
    session.caret = state_.selection().caret;
    session.revision = state_.history().revision();
}

// Vim の面の行のクリック（ADR 0190 の決定 5）。クリックした候補を入れて確定する 1 歩を engine に
// 流す（鍵と同じ写しの道・INSERT の undo の 1 単位の中）。行の名指しが無い確定（Vim のモードでは
// 窓が送らない）は選んでいる語のまま閉じる。打鍵で自動に開いた面（engine の外）の行は、Ctrl-n で
// engine の補完へ移してから同じ 1 歩で確定する（#1971）。
void EditorController::choose_vim_completion(std::optional<std::size_t> row)
{
    if (row.has_value() && state_.vim().insert_completion != nullptr)
    {
        chosen_vim_row(row.value());
    }
    else if (row.has_value() && completion_.has_value() && !completion_.value().by_hand)
    {
        choose_auto_vim_row(row.value());
        return;
    }
    close_completion();
}

// engine の補完の index 番目の候補で確定する 1 歩（面の行のクリック・#1845）。打鍵で自動に開いた面
// から移った補完なら、確定で入る語を先にマクロの録画へ打った本文として積む（#1971）。
void EditorController::chosen_vim_row(std::size_t index)
{
    if (triggered_in_engine())
    {
        record_triggered_typed(index);
    }
    static_cast<void>(stepped_vim(VimStepSource{CompletionChoice{index}}));
}

// 自動で開いた面の index 番目の行で確定する（#1971）。面の候補を Ctrl-n で engine の補完へ移し
// （offer_auto_vim_completion と同じ道・鍵はマクロに積まない）、engine の列の中の同じ候補を確定の
// 1 歩で入れる。engine の語に直せない行（textEdit の範囲が直せない候補）なら面を閉じるだけ。
void EditorController::choose_auto_vim_row(std::size_t index)
{
    const auto rows = auto_vim_rows();
    if (index >= rows.size() || !rows.at(index).has_value())
    {
        close_completion();
        return;
    }
    const auto position = static_cast<std::size_t>(std::count_if(
        rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(index),
        [](const std::optional<core::CompletionEntry> &row) { return row.has_value(); }));
    static_cast<void>(deliver_vim_key(core::VimKey{core::VimSpecialKey::control_n}));
    if (triggered_in_engine())
    {
        chosen_vim_row(position);
    }
    close_completion();
}

bool EditorController::triggered_in_engine() const noexcept
{
    return completion_.has_value() && completion_.value().triggered &&
           completion_.value().by_hand && state_.vim().insert_completion != nullptr;
}

bool EditorController::triggered_completion_key(const core::VimKey &key) const noexcept
{
    if (!completion_.has_value() || !completion_.value().triggered || !completion_.value().by_hand)
    {
        return false;
    }
    const core::VimState &vim = state_.vim();
    return vim.insert_completion != nullptr ? !core::vim_completion_ends(vim, key)
                                            : core::vim_starts_completion(vim, key).has_value();
}

void EditorController::record_triggered_typed(std::optional<std::size_t> index)
{
    if (replay_depth_.has_value() || !state_.vim().macro_recording.has_value())
    {
        return;
    }
    const std::vector<core::VimKey> typed = core::vim_completion_typed(state_.vim(), index);
    if (typed.empty())
    {
        return;
    }
    core::VimState vim = state_.vim();
    core::vim_macro_typed(vim, typed);
    state_ = state_.with_vim(std::move(vim));
}

// 自動で開いた面の Enter と Tab（#1971・施主の決め 2026-11-23）。`map` の左辺を待っている間は確定
// せず、今までどおり表を引く。engine の補完へ移った面は、選んでいる候補があれば Enter も Tab も
// その候補で確定し（Ctrl-y と同じ・Vim に渡さない）、元の接頭辞を選んでいれば Tab だけが先頭の候補
// で確定する。engine の外の面（まだ選んでいない）は Tab だけが先頭の候補で確定する。
bool EditorController::triggered_confirm_took(const core::VimKey &key)
{
    const bool tab = is_tab(key);
    if ((!tab && !is_special(key, core::VimSpecialKey::enter)) ||
        !state_.key_map_pending().empty() || !completion_.has_value() ||
        !completion_.value().triggered)
    {
        return false;
    }
    if (triggered_in_engine())
    {
        return engine_confirm_took(tab);
    }
    if (completion_.value().by_hand || !has_rows(completion_.value()))
    {
        return false;
    }
    // ↑↓ で選んでいれば（#2024）、Enter も Tab もその行で確定する。
    if (completion_.value().stepped)
    {
        choose_auto_vim_row(completion_.value().selected);
        return true;
    }
    return tab && auto_tab_took();
}

// engine の補完へ移った面の Enter と Tab。選んでいる候補があればそれで確定し、元の接頭辞を選んで
// いれば Tab だけが先頭の候補で確定する。選んでいる候補が元の接頭辞と同じ綴りなら確定は本文を変え
// ないので、鍵を Vim に渡す（改行・Tab が空振りしない・通常モードの #1844 の L1 と同じ考え）。
bool EditorController::engine_confirm_took(bool tab)
{
    const auto &engine = *state_.vim().insert_completion;
    if (engine.selected < engine.words->size())
    {
        if (engine.words->at(engine.selected) == engine.original)
        {
            return false;
        }
        close_completion();
        return true;
    }
    if (!tab || engine.words->empty())
    {
        return false;
    }
    chosen_vim_row(0);
    close_completion();
    return true;
}

// engine の外の面の Ctrl-y と Ctrl-e（面を閉じて真）。↑↓ で選んでいる面の Ctrl-y はその行で確定し、
// ほかは本文を変えずに閉じる（#2024）。
bool EditorController::auto_closing_key_took(core::VimSpecialKey pressed)
{
    if (pressed == core::VimSpecialKey::control_y && completion_.has_value() &&
        completion_.value().stepped)
    {
        choose_auto_vim_row(completion_.value().selected);
        return true;
    }
    close_completion();
    return true;
}

// Vim の INSERT の面の ↑↓・PageUp / PageDown（#2024）。engine の補完の中では、↓ は列の向きの鍵
// （Ctrl-n で始めた列は Ctrl-n・Ctrl-p で始めた列は Ctrl-p）・↑ は逆の鍵の 1 歩で、Ctrl-n / Ctrl-p
// を打ったのと同じ道（選ぶたびに本文へ・打鍵で自動に開いた面から移った補完なら録画に積まない）。
// PageDown / PageUp は面の行数ぶん、列の端で止まる（元の接頭辞からの PageDown は先頭の側へ・PageUp
// は動かない）。
void EditorController::step_engine_completion(CompletionStep step)
{
    const auto &engine = *state_.vim().insert_completion;
    const bool forward = engine.direction == core::CompletionDirection::forward;
    const core::VimKey down{forward ? core::VimSpecialKey::control_n
                                    : core::VimSpecialKey::control_p};
    const core::VimKey up{forward ? core::VimSpecialKey::control_p
                                  : core::VimSpecialKey::control_n};
    const std::size_t steps = engine_steps(step, engine.selected, engine.words->size());
    const bool downward = step == CompletionStep::next || step == CompletionStep::page_next;
    for (std::size_t index = 0; index < steps && state_.vim().insert_completion != nullptr; ++index)
    {
        static_cast<void>(deliver_vim_key(downward ? down : up));
    }
}

bool EditorController::completion_takes_arrows() const noexcept
{
    return state_.mode() == core::EditMode::vim && state_.vim().mode == core::VimMode::insert &&
           completion_active() && state_.key_map_pending().empty() &&
           !state_.composition().has_value() && !state_.command_input().has_value();
}

// engine の外の面の Tab。engine の語に直せる候補のいちばん上で確定する（無ければ偽・Vim のまま）。
bool EditorController::auto_tab_took()
{
    const auto rows = auto_vim_rows();
    const auto top = std::ranges::find_if(rows, [](const std::optional<core::CompletionEntry> &row)
                                          { return row.has_value(); });
    if (top == rows.end())
    {
        return false;
    }
    choose_auto_vim_row(static_cast<std::size_t>(top - rows.begin()));
    return true;
}

// 意図の出口の Vim の側（settle_completion から）。engine の補完が閉じたか別の文書になっていれば
// 閉じ、届いた答えを列の先頭側に足す。出す行も待つ答えも無くなったら閉じて 1 行知らせる。
void EditorController::settle_vim_completion(CompletionSession &session)
{
    if (state_.vim().insert_completion == nullptr ||
        state_.autosave_ids().at(state_.active_tab()) != session.document)
    {
        close_completion();
        return;
    }
    static_cast<void>(take_completion_answer(session));
    if (active_language_server().phase != LanguageServerPhase::running)
    {
        session.awaiting = false;
    }
    if (!alive(session))
    {
        close_completion();
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_completion_none));
    }
}

// LSP の候補が入れる本文を、語の頭からキャレットまでと置き換える形にしたもの。`textEdit` の
// 範囲が無ければそのまま。範囲は要求を出したときの座標で、語の頭より前の本文は補完の間に変わら
// ないので、LSP の座標のまま比べる。終わりが要求のときのキャレット（語の頭 ＋ 元の接頭辞）で、
// 始まりが語の頭ならそのまま、語の頭より前（同じ行）で入れる本文がその間の本文で始まるなら残り。
// ほかの範囲（終わりがキャレットでない・語の頭より後ろから・別の行）は直せないので空（列に
// 入れない）。改行は engine の形（LF）。
std::optional<std::string>
EditorController::vim_completion_text(const core::LspRange &typed, core::Offset origin,
                                      const core::CompletionEntry &entry) const
{
    std::string text = inserted_text(entry.insert, "\n");
    if (!entry.range.has_value())
    {
        return text;
    }
    const core::LspRange &range = entry.range.value();
    if (range.end != typed.end || range.start.line != typed.start.line ||
        typed.start.character < range.start.character)
    {
        return std::nullopt;
    }
    const auto begin =
        core::lsp_offset_of(state_.text(), range.start, active_language_server().encoding);
    if (!begin.has_value())
    {
        return std::nullopt;
    }
    const std::string before = state_.text().text_range(begin.value(), origin);
    return text.starts_with(before) ? std::optional{text.substr(before.size())} : std::nullopt;
}

// 要求を出したときの「語の頭からキャレットまで」の LSP の範囲。語の頭より前の本文は補完の間に
// 変わらないので語の頭は今の本文から引き、キャレットは語の頭 ＋ 元の接頭辞の長さ（LSP の単位）。
// 答えが届いたときに 1 回だけ作る。元の接頭辞が本文に読めなければ空。
std::optional<core::LspRange> EditorController::vim_completion_span(core::Offset origin,
                                                                    std::string_view original) const
{
    const auto encoding = active_language_server().encoding;
    const auto typed = core::TextBuffer::from_utf8(original);
    if (!typed.has_value())
    {
        return std::nullopt;
    }
    const core::LspPosition head = core::lsp_position_of(state_.text(), origin, encoding);
    const std::size_t units =
        core::lsp_position_of(typed.value(), core::Offset{original.size()}, encoding).character;
    return core::LspRange{head, core::LspPosition{head.line, head.character + units}};
}

// 後から届いた LSP の候補を列の先頭側に足す（ADR 0190 の決定 5）。元の接頭辞に当たり（filter の
// 前方一致）、語の頭から置き換える形に直せる候補だけを入れる。選んでいる語は本文に入っているので
// 動かさず、engine が新しい列の中の同じ綴りを選び直す（vim_completion_reoffered）。
void EditorController::merge_vim_completion_answer(CompletionSession &session,
                                                   const core::LspCompletionList &list)
{
    session.awaiting = false;
    const auto &engine = state_.vim().insert_completion;
    if (engine == nullptr)
    {
        return;
    }
    const auto typed = vim_completion_span(session.origin, engine->original);
    if (!typed.has_value())
    {
        return;
    }
    const std::vector<core::CompletionEntry> answered = core::completion_entries_of(list);
    std::vector<core::CompletionEntry> server;
    for (const std::size_t position : core::completion_matches(answered, engine->original))
    {
        core::CompletionEntry entry = answered.at(position);
        auto text = vim_completion_text(typed.value(), session.origin, entry);
        if (text.has_value())
        {
            entry.insert = std::move(text).value();
            entry.range.reset();
            server.push_back(std::move(entry));
        }
    }
    if (server.empty())
    {
        return;
    }
    session.server = std::move(server);
    arrange_vim(session);
    state_ = state_.with_vim(core::vim_completion_reoffered(state_.vim(), vim_words(session)));
    follow_vim_completion();
}

// ------------------------------------------------- Vim の INSERT で打鍵が自動に開く面（#1962）

// 言語サーバーが起きている窓か、打ったときの粒（ADR 0192 の追補・#1979）がオンの窓で打った Vim の鍵
// （ADR 0190 の追補・#1962）。打った鍵の入口（accept(VimKeyPress)）だけが呼ぶので、再生（`.`
// `@`）・:normal!（press_vim_keys）・IME の確定（type_as_vim_keys）の鍵はここを通らない。`map` の右
// 辺は鍵の出どころで見分ける（watched_vim_key）。engine の歩は変えない。
void EditorController::typed_vim_key_with_language(const core::VimKey &key)
{
    // 粒だけがオンで言語サーバーが起きていない窓は自動の面の鍵を読まない（粒がオフのときと同じ道・
    // #1988 のレビュー L2）。
    // 自動で開いた面の Enter と Tab の確定は `map` の表より先（#1971）。
    if (language_servers_.awake != 0 &&
        (triggered_confirm_took(key) || auto_vim_completion_took(key)))
    {
        return;
    }
    // 言語サーバーが running でなく粒もオフなら見張らない（1 文字ごとに文字列を作らない）。
    const bool language = active_language_server().phase == LanguageServerPhase::running;
    {
        const ScopedValue<bool> watch{typed_watch_, language || state_.settings().caret_sparks};
        typed_insert_.reset();
        static_cast<void>(typed_vim_key(key));
    }
    typed_vim_character(language);
}

// engine に流す鍵の 1 つ（deliver_vim_key の頭・typed_watch_ の間だけ）。打った文字として
// 数えるのは、出どころが typed（map の右辺の単位は typed_unit が mapped にする・KeyMapEmit の
// mapped）で再生の中でなく、流す前の engine が INSERT で 1 鍵の引数（Ctrl-r のレジスタ・
// Ctrl-v の文字）を待っていない文字の鍵だけ。左辺の待ちの後に一度に流れた鍵
// （`inoremap jk <Esc>` の `j.`）も、最後に流した `.` を、それを流す前のキャレットで数える。
void EditorController::watched_vim_key(const core::VimKey &key)
{
    typed_insert_.reset();
    const auto *character = std::get_if<core::VimCharacter>(&key);
    const core::VimState &vim = state_.vim();
    if (character == nullptr || unit_source_ != core::VimKeySource::typed ||
        replay_depth_.has_value() || state_.command_input().has_value() ||
        state_.mode() != core::EditMode::vim || vim.mode != core::VimMode::insert ||
        vim.input_wait.has_value())
    {
        return;
    }
    typed_insert_ = std::pair{*character, state_.selection().caret};
}

bool EditorController::auto_vim_completion_took(const core::VimKey &key)
{
    // engine の補完へ移った面の鍵は engine の道（triggered_step_vim・#1971）。
    if (!completion_.has_value() || completion_.value().by_hand)
    {
        return false;
    }
    const auto *special = std::get_if<core::VimSpecialKey>(&key);
    if (special == nullptr)
    {
        return false;
    }
    const core::VimSpecialKey pressed = *special;
    // 何も選んでいない面の Ctrl-y（確定）と Ctrl-e（取り消し）は、本文を変えずに面を閉じる。↑↓ で
    // 選んでいる面の Ctrl-y はその行で確定する（#2024）。答えを待っているだけ（面がまだ出ていない）
    // なら Vim のまま。
    if (pressed == core::VimSpecialKey::control_y || pressed == core::VimSpecialKey::control_e)
    {
        return has_rows(completion_.value()) && auto_closing_key_took(pressed);
    }
    if (pressed != core::VimSpecialKey::escape && pressed != core::VimSpecialKey::backspace &&
        pressed != core::VimSpecialKey::control_n && pressed != core::VimSpecialKey::control_p)
    {
        close_completion();
    }
    return false;
}

// 鍵の後も INSERT で、最後に流した打った文字が流す前のキャレットの直後にそのまま入ったときだけ（1
// 文字で抜ける鍵・貼り付け・1 鍵の引数で入った文字は渡さない）。粒がオンなら打鍵の通し番号を進め
// （#1979）、language（鍵を流す前に言語サーバーが running だった）なら言語サーバーへ渡す。
void EditorController::typed_vim_character(bool language)
{
    if (!typed_insert_.has_value())
    {
        return;
    }
    const auto [character, before] = typed_insert_.value();
    typed_insert_.reset();
    if (state_.mode() != core::EditMode::vim || state_.vim().mode != core::VimMode::insert)
    {
        return;
    }
    std::string typed;
    core::append_utf8(typed, character.code);
    const core::Offset caret = state_.selection().caret;
    if (caret.value != before.value + typed.size() ||
        state_.text().text_range(before, caret) != typed)
    {
        return;
    }
    if (state_.settings().caret_sparks)
    {
        ++typed_serial_;
    }
    if (language)
    {
        typed_for_language(typed);
    }
}

// 自動の面の候補を engine の写しにする（ADR 0169 の形へ移る）。写すのは面に出ている LSP の候補
// （今の接頭辞で絞った列）だけで、要求は出し直さず、待っている要求と届いていた答えは捨てる。以後の
// 面は engine が持つので手で開いた面として扱う（by_hand を立てる）。engine の語に直すのは手で開いた
// 面と同じ vim_completion_text の 1 本で、比べる範囲は最後の要求を出したときの接頭辞（requested）。
bool EditorController::offer_auto_vim_completion()
{
    if (!completion_.has_value())
    {
        return false;
    }
    CompletionSession &session = completion_.value();
    std::vector<core::CompletionEntry> server;
    for (std::optional<core::CompletionEntry> &row : auto_vim_rows())
    {
        if (row.has_value())
        {
            server.push_back(std::move(row).value());
        }
    }
    if (server.empty())
    {
        return false;
    }
    for (const LanguageServerSlot slot : language_server_slots)
    {
        cancel_lsp_requests(slot, LspRequestKind::completion);
    }
    forget_completion_answer();
    session.server = std::move(server);
    session.by_hand = true;
    session.awaiting = false;
    session.incomplete = false;
    arrange_vim(session);
    state_ = state_.with_vim(core::vim_completion_offered(
        state_.vim(), core::VimCompletionOffer{vim_words(session), session.origin}));
    return true;
}

// 自動で開いた面の行（shown の順）ごとに、engine の語に直した候補。直すのは手で開いた面と同じ
// vim_completion_text の 1 本で、比べる範囲は最後の要求を出したときの接頭辞（requested）。
std::vector<std::optional<core::CompletionEntry>> EditorController::auto_vim_rows() const
{
    std::vector<std::optional<core::CompletionEntry>> rows;
    if (!completion_.has_value())
    {
        return rows;
    }
    const CompletionSession &session = completion_.value();
    const auto typed = vim_completion_span(session.origin, session.requested);
    rows.reserve(session.shown.size());
    for (const std::size_t position : session.shown)
    {
        core::CompletionEntry entry = session.entries.at(position);
        auto text = typed.has_value() ? vim_completion_text(typed.value(), session.origin, entry)
                                      : std::nullopt;
        if (!text.has_value())
        {
            rows.emplace_back(std::nullopt);
            continue;
        }
        entry.insert = std::move(text).value();
        entry.range.reset();
        rows.emplace_back(std::move(entry));
    }
    return rows;
}

std::optional<CompletionView> EditorController::completion_view() const
{
    if (!completion_.has_value() || !has_rows(completion_.value()))
    {
        return std::nullopt;
    }
    const auto &session = completion_.value();
    const std::size_t count =
        std::min(core::completion_face_rows, session.shown.size() - session.first);
    // 選んでいる行が無い（Vim の補完が元の接頭辞へ戻った 1 歩・#1845、打鍵で自動に開いてまだ選んで
    // いない面・#1962・#1971）ときは rows の外の値。
    const bool unchosen = awaits_choice(session);
    CompletionView view{.session = session.id,
                        .rows = {},
                        .selected = !unchosen && session.selected < session.shown.size()
                                        ? session.selected - session.first
                                        : count,
                        .first = session.first,
                        .total = session.shown.size(),
                        .detailed = session.detailed};
    view.rows.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto &entry = session.entries.at(session.shown.at(session.first + index));
        view.rows.push_back(CompletionRowView{entry.label, entry.detail, entry.source});
    }
    return view;
}
} // namespace nenenib::application
