#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "AutosaveOutcome.hpp"
#include "AutosavePlan.hpp"
#include "AutosaveSnapshot.hpp"
#include "AutosaveStep.hpp"
#include "DisplayText.hpp"
#include "DocumentState.hpp"
#include "Language.hpp"
#include "SaveState.hpp"
#include "Selection.hpp"
#include "TabTitle.hpp"
#include "TextPosition.hpp"
#include "UnloadedDocument.hpp"
#include "VimMarkEdits.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
using detail::line_within;
using detail::named_notice;

namespace
{
// 自動退避する本文の出どころ（ADR 0137 の決定 2）。まだ読んでいない文書は退避しない。写し先が
// 足りなければ std::visit が落ちる（CPP-002）。
[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
autosave_bundle(const std::shared_ptr<const DocumentState> &tab)
{
    return tab;
}

[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
autosave_bundle(const std::shared_ptr<const UnloadedDocument> &)
{
    return std::nullopt;
}

// 断片を 1 本のバイト列に写す（自動退避の写し・決定 4）。
[[nodiscard]] std::vector<std::byte> joined_bytes(std::span<const std::string_view> parts)
{
    std::size_t size = 0;
    for (const std::string_view part : parts)
    {
        size += part.size();
    }
    std::vector<std::byte> bytes;
    bytes.reserve(size);
    for (const std::string_view part : parts)
    {
        const auto view = std::as_bytes(std::span{part});
        bytes.insert(bytes.end(), view.begin(), view.end());
    }
    return bytes;
}

// 自動退避の失敗の 1 行（ADR 0137 の決定 4）。
constexpr std::string_view autosave_failed_message = "自動退避できませんでした";
// 上限を超える本文を退避しない知らせ（ADR 0159 の決定 7・L-4）。
constexpr std::string_view autosave_oversized_message = "64 MiB を超える文書は自動退避しません";
// 自動退避の残骸を読めなかった知らせ（ADR 0137 の決定 6）。
constexpr std::string_view unrestored_prefix = "戻せませんでした: ";

// 読めなかった残骸の知らせ。名前は未保存の印の無い題名（無題なら「無題」）。
[[nodiscard]] core::DisplayText unrestored_message(const AutosaveRemnant &remnant,
                                                   std::size_t others)
{
    return named_notice(
        unrestored_prefix,
        std::string(core::tab_title_for(remnant.path, core::SaveState::saved).text()), others);
}

// 自動退避から戻した 1 タブの束（ADR 0137 の決定 6）。保存した時点は無い（未保存の印）。
// キャレットは一覧に書いた位置を reached_bundle と同じく寄せる（行は本文の範囲へ、桁は行の中の
// 文字の境目へ）。スクロールは先頭、履歴は空。
[[nodiscard]] DocumentState restored_document(core::TextBuffer text, Document document,
                                              core::TextPosition caret)
{
    const core::Offset placed =
        text.offset_of(core::TextPosition{line_within(text, caret.line), caret.column});
    const core::VimMarkPosition opened = core::vim_mark_position_of(text, placed);
    DocumentView view{core::tab_title_for(document.path, core::SaveState::modified), document.path,
                      document.encoding, core::SaveState::modified, std::nullopt};
    // 戻した文書の言語もパスから引く（ADR 0154 の決定 5）。
    const core::Language language = language_of(document);
    DocumentState bundle{std::move(text),
                         core::collapsed_at(placed),
                         core::EditHistory::empty(),
                         std::move(document),
                         core::LineNumber{1},
                         std::nullopt,
                         std::nullopt,
                         std::move(view)};
    bundle.language = language;
    // 戻したキャレットをジャンプの列に 1 項持って始める（reached_bundle と同じ・ADR 0152 の追補
    // #1124）。
    bundle.marks = core::vim_marks_opened(opened);
    return bundle;
}
} // namespace

// 自動退避の合図（ADR 0137 の決定 1・2）。入口は歩きも知らせも残す。退避を頼まなかった合図は
// 何も変えないので描き直さない（ADR 0155 の追補・#813）。
void EditorController::accept(const AutosaveTick &)
{
    repaint_ = false;
    autosave_now();
}

// 計画は純関数で、帳面を更新するのはこの道だけ（合図と復元の直後）。本文の写しは UI スレッドで
// 作り、書くのは worker（決定 4）。
void EditorController::autosave_now()
{
    for (const auto &[id, step] : autosave_plan(state_, autosave_ledger_))
    {
        switch (step)
        {
        case AutosaveStep::store:
            repaint_ = true;
            store_planned(id);
            break;
        case AutosaveStep::discard:
            ports_.autosave.discard(id);
            autosave_ledger_.erase(id);
            break;
        case AutosaveStep::keep:
            break;
        case AutosaveStep::oversized:
            notify_autosave_oversized(id);
            break;
        }
    }
}

// 起動の問いの前の一覧（ADR 0137 の決定 6・7）。起動の道で自動退避に触れるのはこの 1 回だけ。
// 読めなければ戻すものは無い（問わない）。
const std::vector<AutosaveRemnant> &EditorController::autosave_remnants()
{
    autosave_remnants_ = ports_.autosave.remnants().value_or(std::vector<AutosaveRemnant>{});
    return autosave_remnants_;
}

// 「はい」（決定 6）。覚えた残骸を順に読み、戻せた分を帯の末尾へ足してから、最初の 1 本へ
// 切り替える。読めない残骸は知らせて飛ばす。戻したタブは次の合図を待たずにこのプロセスの退避と
// して頼み、その後にどの残骸も forget する（頼んだ書きが終わるまでの隙間は ADR の限界）。
void EditorController::accept(const RestoreAutosave &)
{
    const std::vector<AutosaveRemnant> remnants = std::exchange(autosave_remnants_, {});
    std::vector<DocumentState> restored;
    std::size_t unrestored = 0;
    for (const AutosaveRemnant &remnant : remnants)
    {
        auto bundle = restored_bundle(remnant);
        if (bundle.has_value())
        {
            restored.push_back(std::move(bundle).value());
            continue;
        }
        state_ = state_.with_command_message(unrestored_message(remnant, unrestored++));
    }
    if (!restored.empty())
    {
        const std::size_t first = state_.tab_count();
        state_ = state_.with_appended(std::move(restored));
        leave_document();
        state_ = state_.with_switched(first);
        enter_document();
        autosave_now();
    }
    for (const AutosaveRemnant &remnant : remnants)
    {
        ports_.autosave.forget(remnant);
    }
}

// 「いいえ」（決定 6）。本文を読まずに全部 forget する。
void EditorController::accept(const DiscardAutosave &)
{
    for (const AutosaveRemnant &remnant : std::exchange(autosave_remnants_, {}))
    {
        ports_.autosave.forget(remnant);
    }
}

// 開くのと同じ判別と復号（read_document と同じ DetectedText → decoded）。一覧のパスの符号化は
// 使わず、本文の判別が決める。
std::optional<DocumentState> EditorController::restored_bundle(const AutosaveRemnant &remnant)
{
    const auto bytes = ports_.autosave.read(remnant);
    if (!bytes)
    {
        return std::nullopt;
    }
    const std::span<const std::byte> body = bytes.value();
    auto detected = core::DetectedText::from_bytes(
        std::string_view{std::bit_cast<const char *>(body.data()), body.size()});
    if (!detected)
    {
        return std::nullopt;
    }
    const core::TextEncoding encoding = detected.value().encoding();
    auto text = decoded(std::move(detected).value());
    if (!text)
    {
        return std::nullopt;
    }
    return restored_document(std::move(text).value(),
                             Document{remnant.path, encoding, std::nullopt}, remnant.caret);
}

void EditorController::store_planned(AutosaveId id)
{
    const auto &ids = state_.autosave_ids();
    const auto position =
        static_cast<std::size_t>(std::ranges::distance(ids.begin(), std::ranges::find(ids, id)));
    if (position == state_.active_tab())
    {
        if (store_autosave(id, state_.text(), state_.document(),
                           state_.text().position_of(state_.selection().caret)))
        {
            autosave_ledger_.insert_or_assign(id, state_.history().revision());
        }
        return;
    }
    const auto bundle =
        std::visit([](const auto &tab) { return autosave_bundle(tab); },
                   state_.parked().at(position < state_.active_tab() ? position : position - 1));
    if (!bundle.has_value())
    {
        return;
    }
    const DocumentState &tab = *bundle.value();
    if (store_autosave(id, tab.text, tab.document, tab.text.position_of(tab.selection.caret)))
    {
        autosave_ledger_.insert_or_assign(id, tab.history.revision());
    }
}

bool EditorController::store_autosave(AutosaveId id, const core::TextBuffer &text,
                                      const Document &document, core::TextPosition caret)
{
    std::string held;
    const auto parts = encoded_parts(text, document.encoding, held);
    if (!parts)
    {
        notify_autosave_failure();
        return false;
    }
    // store にするのは未保存のタブだけなので、題名は未保存の印つき。
    ports_.autosave.store(AutosaveSnapshot{
        id, document.path, core::tab_title_for(document.path, core::SaveState::modified),
        joined_bytes(parts.value()), caret});
    return true;
}

// worker が終えた退避の結果を全部引く（決定 4・独立レビュー F3）。失敗した id は帳面から外し、
// 本文が変わらなくても次の合図で書き直す。失敗が 1 つでもあれば（まだ出していなければ）1 回
// 知らせ、失敗が無く成功が 1 つでもあれば知らせの印を下ろす（次の失敗でまた 1 回出る）。
// 結果が無ければ（フォルダの合図）何も変えない。
void EditorController::collect_autosave()
{
    const auto outcomes = ports_.autosave.collect();
    bool failed = false;
    for (const AutosaveOutcome &outcome : outcomes)
    {
        if (outcome.failure.has_value())
        {
            failed = true;
            autosave_ledger_.erase(outcome.id);
        }
    }
    if (failed)
    {
        notify_autosave_failure();
        return;
    }
    if (!outcomes.empty())
    {
        autosave_notified_ = false;
    }
}

void EditorController::notify_autosave_failure()
{
    if (autosave_notified_)
    {
        return;
    }
    autosave_notified_ = true;
    // 材料は固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    reject(core::DisplayText::parse(autosave_failed_message).value());
}

// 退避しない知らせは文書ごとに 1 回（ADR 0159 の決定 7・L-4）。打鍵の失敗ではないので鳴らさない。
void EditorController::notify_autosave_oversized(AutosaveId id)
{
    if (std::ranges::find(autosave_oversized_, id) != autosave_oversized_.end())
    {
        return;
    }
    autosave_oversized_.push_back(id);
    repaint_ = true;
    // 材料は固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    state_ =
        state_.with_command_message(core::DisplayText::parse(autosave_oversized_message).value());
}
} // namespace nenenib::application
