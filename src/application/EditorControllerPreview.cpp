#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CaretMotion.hpp"
#include "ClipboardOperation.hpp"
#include "DisplayText.hpp"
#include "EditBoundary.hpp"
#include "EditorIntent.hpp"
#include "ExPreviewRequest.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownRequest.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PreviewBlock.hpp"
#include "PreviewFullRoute.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewPoint.hpp"
#include "PreviewSelection.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewView.hpp"
#include "ScrollState.hpp"
#include "SelectionAnchoring.hpp"
#include "SetPreviewPlacement.hpp"
#include "TogglePreviewTask.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// プレビューの 1 行（ADR 0155 の決定 3・6）。
constexpr std::string_view preview_too_large_message = "プレビューは 16 MiB までです";
constexpr std::string_view preview_rejected_message = "Markdown を解析できませんでした";
constexpr std::string_view preview_memory_message = "メモリが足りずプレビューできませんでした";
// チェックボックスの印が本文で `[ ]` `[x]` `[X]` の形でないとき（ADR 0155 の追補 24）。
constexpr std::string_view preview_task_stale_message = "プレビューが古いので切り替えられません";

// 見せている結果の block の印の位置（タスクでない・範囲の外なら nullopt）。
[[nodiscard]] std::optional<std::size_t> task_mark_of(const PreviewView &preview, std::size_t block)
{
    if (preview.document == nullptr || block >= preview.document->blocks.size())
    {
        return std::nullopt;
    }
    const auto &task = preview.document->blocks.at(block).task;
    if (!task.has_value())
    {
        return std::nullopt;
    }
    return task.value().mark_offset;
}

// 印の前後を含む 3 バイトが `[ ]` `[x]` `[X]` か。
[[nodiscard]] bool task_mark_shape(std::string_view around) noexcept
{
    return around.size() == 3U && around.front() == '[' && around.back() == ']' &&
           (around.at(1) == ' ' || around.at(1) == 'x' || around.at(1) == 'X');
}
// 全面プレビューの間の知らせ（ADR 0155 の改訂 17）。鳴らさない。
constexpr std::string_view preview_full_refused_message =
    "プレビュー中は編集できません。Esc で戻る";
// 全面の Space で切り替えるタスクがキャレットの行に無いとき（追補 25）。
constexpr std::string_view preview_no_task_message = "この行にタスクはありません";

// 全面プレビューの間も写す意図のうち、タブ・窓・枠・面の側（passes_full_preview の続き・
// CPP-012 の関数の長さで分ける）。
[[nodiscard]] bool passes_full_preview_window(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<NewTab>(intent) || std::holds_alternative<SwitchTab>(intent) ||
           std::holds_alternative<WalkRecentTab>(intent) ||
           std::holds_alternative<SettleRecentTab>(intent) ||
           std::holds_alternative<CloseTab>(intent) || std::holds_alternative<ScrollTabs>(intent) ||
           std::holds_alternative<OpenTabList>(intent) ||
           std::holds_alternative<EndSession>(intent) ||
           std::holds_alternative<RestoreAutosave>(intent) ||
           std::holds_alternative<DiscardAutosave>(intent) ||
           std::holds_alternative<ToggleBookmark>(intent) ||
           std::holds_alternative<OpenOperationList>(intent) ||
           std::holds_alternative<SelectDisplay>(intent) ||
           std::holds_alternative<SelectLanguage>(intent) ||
           std::holds_alternative<SelectSyntax>(intent) ||
           std::holds_alternative<ScrollPaletteList>(intent) ||
           std::holds_alternative<SplitPane>(intent) ||
           std::holds_alternative<SwitchPane>(intent) ||
           std::holds_alternative<ClosePane>(intent) ||
           std::holds_alternative<TogglePreview>(intent) ||
           std::holds_alternative<TogglePreviewTask>(intent) ||
           std::holds_alternative<SetPreviewPlacement>(intent) ||
           std::holds_alternative<ToggleExplorer>(intent) ||
           std::holds_alternative<ExplorerKey>(intent) ||
           std::holds_alternative<PointExplorer>(intent) ||
           std::holds_alternative<ScrollExplorer>(intent) ||
           std::holds_alternative<ExplorerRootChosen>(intent);
}

// 全面プレビューの間も今までどおり写す意図（ADR 0155 の改訂 17）。タブ（帯・Ctrl+Tab・`gt` は鍵の
// 側）・Ctrl+P と入力行・F1・窓と枠と面の操作・保存と開く・表示の選択・合図と裏の仕事・
// チェックボックスのクリック（追補 24 の唯一の例外）。使う人の操作でない意図（合図・裏の仕事・窓の
// 寸法）は not_user_operation の 1 本を経て全部通す。ここに無い意図は key_route が鍵ごとに決め、
// どれにも当たらなければ本文に届かない（refuse）。新しい意図を足すときは、ここか key_route に書く。
[[nodiscard]] bool passes_full_preview(const EditorIntent &intent) noexcept
{
    return detail::not_user_operation(intent) || std::holds_alternative<SelectEditMode>(intent) ||
           std::holds_alternative<OpenDocument>(intent) ||
           std::holds_alternative<SaveDocument>(intent) ||
           std::holds_alternative<CancelComposition>(intent) ||
           std::holds_alternative<AdjustFontSize>(intent) ||
           std::holds_alternative<CommandText>(intent) ||
           std::holds_alternative<EditCommand>(intent) ||
           std::holds_alternative<SubmitCommand>(intent) ||
           std::holds_alternative<CancelCommand>(intent) ||
           std::holds_alternative<PasteCommand>(intent) ||
           std::holds_alternative<OpenCommandPalette>(intent) ||
           std::holds_alternative<ActivateCommandChoice>(intent) ||
           std::holds_alternative<SearchHop>(intent) || passes_full_preview_window(intent);
}

// 通常モードの矢印・PgUp / PgDn・Ctrl+Home / End（改訂 17）。左右と行頭・行末・語は届くが何も
// しない。Shift つきは選択（N5b）。表は CaretMotion の全値を 1 回ずつ持つ。
using PreviewMotionRow = std::pair<core::CaretMotion, PreviewFullRoute>;
constexpr std::array<PreviewMotionRow, 12> preview_motions{{
    {core::CaretMotion::previous_character, PreviewFullRoute::ignore},
    {core::CaretMotion::next_character, PreviewFullRoute::ignore},
    {core::CaretMotion::previous_word, PreviewFullRoute::ignore},
    {core::CaretMotion::next_word, PreviewFullRoute::ignore},
    {core::CaretMotion::previous_line, PreviewFullRoute::line_up},
    {core::CaretMotion::next_line, PreviewFullRoute::line_down},
    {core::CaretMotion::line_start, PreviewFullRoute::ignore},
    {core::CaretMotion::line_end, PreviewFullRoute::ignore},
    {core::CaretMotion::page_up, PreviewFullRoute::page_up},
    {core::CaretMotion::page_down, PreviewFullRoute::page_down},
    {core::CaretMotion::document_start, PreviewFullRoute::top},
    {core::CaretMotion::document_end, PreviewFullRoute::bottom},
}};

// Vim の NORMAL の文字の鍵（改訂 17）。`:` は Ex の入力行を開く（engine へ）。
using PreviewCharacterRow = std::pair<char32_t, PreviewFullRoute>;
constexpr std::array<PreviewCharacterRow, 6> preview_vim_characters{{
    {U'j', PreviewFullRoute::line_down},
    {U'k', PreviewFullRoute::line_up},
    {U'G', PreviewFullRoute::bottom},
    {U'g', PreviewFullRoute::vim_g},
    {U':', PreviewFullRoute::pass},
    {U'y', PreviewFullRoute::copy},
}};

// Vim の特別な鍵（改訂 17）。Esc はどのモードでも戻る。表に無い鍵は本文に届かない。
using PreviewSpecialRow = std::pair<core::VimSpecialKey, PreviewFullRoute>;
constexpr std::array<PreviewSpecialRow, 14> preview_vim_specials{{
    {core::VimSpecialKey::escape, PreviewFullRoute::leave},
    {core::VimSpecialKey::arrow_down, PreviewFullRoute::line_down},
    {core::VimSpecialKey::arrow_up, PreviewFullRoute::line_up},
    {core::VimSpecialKey::arrow_left, PreviewFullRoute::ignore},
    {core::VimSpecialKey::arrow_right, PreviewFullRoute::ignore},
    {core::VimSpecialKey::home, PreviewFullRoute::ignore},
    {core::VimSpecialKey::end, PreviewFullRoute::ignore},
    {core::VimSpecialKey::page_down, PreviewFullRoute::page_down},
    {core::VimSpecialKey::page_up, PreviewFullRoute::page_up},
    {core::VimSpecialKey::control_d, PreviewFullRoute::half_down},
    {core::VimSpecialKey::control_u, PreviewFullRoute::half_up},
    {core::VimSpecialKey::control_f, PreviewFullRoute::page_down},
    {core::VimSpecialKey::control_b, PreviewFullRoute::page_up},
    {core::VimSpecialKey::control_a, PreviewFullRoute::select_all},
}};

template <typename Row, typename Key>
[[nodiscard]] PreviewFullRoute route_in(std::span<const Row> rows, Key key,
                                        PreviewFullRoute missing) noexcept
{
    const auto found = std::ranges::find(rows, key, &Row::first);
    return found == rows.end() ? missing : found->second;
}

// Vim の鍵の行き先。文字の鍵は NORMAL のときだけ表を引き、INSERT と VISUAL の文字は本文に
// 届かない。確定した検索（VimSearchPattern）も届かない。
[[nodiscard]] PreviewFullRoute vim_key_route(const core::VimKey &key, core::VimMode mode) noexcept
{
    if (const auto *special = std::get_if<core::VimSpecialKey>(&key); special != nullptr)
    {
        return route_in(std::span<const PreviewSpecialRow>(preview_vim_specials), *special,
                        PreviewFullRoute::refuse);
    }
    const auto *character = std::get_if<core::VimCharacter>(&key);
    if (character == nullptr || mode != core::VimMode::normal)
    {
        return PreviewFullRoute::refuse;
    }
    return route_in(std::span<const PreviewCharacterRow>(preview_vim_characters), character->code,
                    PreviewFullRoute::refuse);
}

// 通常モードの矢印の行き先。Shift つきは選択（N5b）。
[[nodiscard]] PreviewFullRoute motion_route(const MoveCaret &move) noexcept
{
    return move.anchoring == core::SelectionAnchoring::extend
               ? PreviewFullRoute::select
               : route_in(std::span<const PreviewMotionRow>(preview_motions), move.motion,
                          PreviewFullRoute::ignore);
}

// Esc とホイールとマウスの行き先。Esc は通常モードでは CancelSelection。ホイールは本文の上でも
// もう 1 つの枠の上でも錨を送る。Ctrl+A と全面のマウス（PointPreview）は選択（改訂 18）。本文の
// マウスの意図（PlaceCaret・PointText）は全面の間は ui が送らず、届いても何もしない。
[[nodiscard]] PreviewFullRoute pointer_route(const EditorIntent &intent) noexcept
{
    if (std::holds_alternative<CancelSelection>(intent))
    {
        return PreviewFullRoute::leave;
    }
    if (std::holds_alternative<ScrollLines>(intent) || std::holds_alternative<ScrollPane>(intent))
    {
        return PreviewFullRoute::wheel;
    }
    if (std::holds_alternative<SelectAll>(intent))
    {
        return PreviewFullRoute::select_all;
    }
    if (std::holds_alternative<PointPreview>(intent))
    {
        return PreviewFullRoute::select;
    }
    const bool body_pointer =
        std::holds_alternative<PlaceCaret>(intent) || std::holds_alternative<PointText>(intent);
    return body_pointer ? PreviewFullRoute::ignore : PreviewFullRoute::refuse;
}

// 通常モードのクリップボードと文字の行き先（ほかは pointer_route）。コピーは選択の口（N5b）、
// 通常モードの Space は追補 25 の切り替え。
[[nodiscard]] PreviewFullRoute other_route(const EditorIntent &intent, core::EditMode mode) noexcept
{
    if (const auto *clipboard = std::get_if<ClipboardAction>(&intent); clipboard != nullptr)
    {
        return clipboard->operation == ClipboardOperation::copy ? PreviewFullRoute::copy
                                                                : PreviewFullRoute::refuse;
    }
    if (const auto *text = std::get_if<InsertText>(&intent); text != nullptr)
    {
        return mode == core::EditMode::ordinary && text->utf8 == " " ? PreviewFullRoute::task
                                                                     : PreviewFullRoute::refuse;
    }
    return pointer_route(intent);
}

// passes_full_preview に当たらなかった意図の行き先。Esc は Vim では VimKeyPress の Esc。
[[nodiscard]] PreviewFullRoute key_route(const EditorIntent &intent, core::EditMode mode,
                                         core::VimMode vim_mode) noexcept
{
    if (const auto *press = std::get_if<VimKeyPress>(&intent); press != nullptr)
    {
        return vim_key_route(press->key, vim_mode);
    }
    if (const auto *move = std::get_if<MoveCaret>(&intent); move != nullptr)
    {
        return motion_route(*move);
    }
    return other_route(intent, mode);
}

[[nodiscard]] PreviewFullRoute preview_full_route(const EditorIntent &intent, core::EditMode mode,
                                                  core::VimMode vim_mode) noexcept
{
    return passes_full_preview(intent) ? PreviewFullRoute::pass : key_route(intent, mode, vim_mode);
}

// ホイールの行数（route が wheel のときだけ呼ぶ）。
[[nodiscard]] std::int64_t wheel_lines(const EditorIntent &intent) noexcept
{
    if (const auto *pane = std::get_if<ScrollPane>(&intent); pane != nullptr)
    {
        return pane->lines;
    }
    const auto *lines = std::get_if<ScrollLines>(&intent);
    return lines == nullptr ? 0 : lines->lines;
}
} // namespace

// ---------------------------------------------------------------- プレビュー（ADR 0155）

// 開く・閉じるの 1 本（決定 3）。開く前に、前に開いていたときの残りを捨てる（たまりが空でないと
// 次の合図が来ない・ADR 0062 の決定 15 と同じ）。
void EditorController::accept(const TogglePreview &)
{
    // 全面の間は側の鍵で側へ戻す（ADR 0155 の改訂 16・もう一度で閉じる）。
    if (state_.full_preview() != nullptr)
    {
        leave_full_preview(PreviewPlacement::side);
        return;
    }
    if (state_.preview() != nullptr)
    {
        state_ = state_.without_split();
        return;
    }
    static_cast<void>(ports_.markdown.collect());
    settle_follow();
    state_ = state_.with_preview(PreviewView{});
    request_preview(true);
}

// チェックボックスの切り替え（ADR 0155 の追補 24）。見せている結果の印の位置を今の本文で確かめ、
// `[ ]` なら `x`、`[x]` `[X]` なら ` ` に置き換える Edit 1 つ（undo 1 単位・restore はそのときの
// キャレット）。プレビューの枠を押しても本文のキャレットは動かないので（ADR 0155）、選択と追従の
// 頼みは編集の前のまま残す（1 バイトを 1 バイトに置き換えるので位置はずれない）。描き直しは改訂
// 番号が進んだ本文を PreviewTick が頼む道（決定 8）で、新しい要求の道は足さない。
void EditorController::accept(const TogglePreviewTask &intent)
{
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr)
    {
        return;
    }
    const auto mark = task_mark_of(*preview, intent.block);
    const std::size_t size = state_.text().size_bytes();
    if (!mark.has_value() || mark.value() == 0U || mark.value() + 2U > size ||
        !task_mark_shape(state_.text().text_range(core::Offset{mark.value() - 1U},
                                                  core::Offset{mark.value() + 2U})))
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse(preview_task_stale_message).value());
        return;
    }
    const core::Offset at{mark.value()};
    const bool checked = state_.text().text_range(at, core::Offset{at.value + 1U}) != " ";
    const auto selection = state_.selection();
    const auto follow = pending_follow_;
    replace(core::OffsetRange{at, core::Offset{at.value + 1U}}, checked ? " " : "x",
            core::EditBoundary::separate);
    state_ = state_.with_selection(selection);
    pending_follow_ = follow;
}

// 打つのを止めてから 1 tick 以上たった本文だけを頼む（決定 8）。比べるのは履歴の位置ではなく
// 改訂番号。頼まなかった合図は見えるものを変えない（見た改訂番号は描かない）ので描き直さない
// （ADR 0155 の追補・#813）。結果を採るのは WorkCompleted の道。
void EditorController::accept(const PreviewTick &)
{
    repaint_ = false;
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr)
    {
        return;
    }
    const std::size_t revision = state_.history().revision();
    if (revision == preview->seen_revision && revision != preview->requested_revision)
    {
        repaint_ = true;
        request_preview(false);
        return;
    }
    if (revision != preview->seen_revision)
    {
        PreviewView view = *preview;
        view.seen_revision = revision;
        state_ = state_.with_shown_preview(std::move(view));
    }
}

void EditorController::request_preview(bool fresh)
{
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    const std::size_t revision = state_.history().revision();
    view.requested_revision = revision;
    view.seen_revision = revision;
    if (fresh)
    {
        view.document.reset();
        view.notice.reset();
        view.follows = true;
    }
    // 券は頼まなくても進め、前の要求の結果を捨てる（決定 6）。
    view.ticket = ++preview_tickets_;
    if (state_.text().size_bytes() > core::preview_byte_limit)
    {
        view.notice = core::DisplayText::parse(preview_too_large_message).value();
        state_ = state_.with_shown_preview(std::move(view));
        return;
    }
    auto text = std::make_shared<const std::string>(state_.text().text());
    ports_.markdown.parse(MarkdownRequest{view.ticket, std::move(text)});
    state_ = state_.with_shown_preview(std::move(view));
}

void EditorController::collect_preview()
{
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    bool changed = false;
    for (MarkdownOutcome &outcome : ports_.markdown.collect())
    {
        if (outcome.ticket != view.ticket)
        {
            continue;
        }
        if (outcome.result)
        {
            view.document =
                std::make_shared<const core::MarkdownDocument>(std::move(outcome.result).value());
            view.notice.reset();
            // 選択の位置は前の結果の要素を指すので、差し替えたら捨てる（改訂 18）。
            view.selection.reset();
            changed = true;
            continue;
        }
        // 差し替わるまで前の結果を出したまま、1 行だけ添える（決定 6）。
        switch (outcome.result.error())
        {
        case core::MarkdownFailure::stopped:
            continue;
        case core::MarkdownFailure::too_large:
            view.notice = core::DisplayText::parse(preview_too_large_message).value();
            break;
        case core::MarkdownFailure::parser_rejected:
            view.notice = core::DisplayText::parse(preview_rejected_message).value();
            break;
        case core::MarkdownFailure::insufficient_memory:
            view.notice = core::DisplayText::parse(preview_memory_message).value();
            break;
        }
        changed = true;
    }
    if (changed)
    {
        state_ = state_.with_shown_preview(std::move(view));
    }
}

ScrollState EditorController::framed_scroll()
{
    const ScrollState scroll = settled_scroll();
    follow_preview_on_scroll(scroll.first_visible);
    return scroll;
}

void EditorController::follow_preview_on_scroll(core::LineNumber first_visible)
{
    const auto previous = std::exchange(preview_body_top_, first_visible);
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr || preview->follows || !previous.has_value() ||
        previous.value() == first_visible)
    {
        return;
    }
    PreviewView view = *preview;
    view.follows = true;
    state_ = state_.with_shown_preview(std::move(view));
}

// ---------------------------------------------------------------- 全面プレビュー（ADR 0155 の改訂
// 15〜17）

void EditorController::accept(const SetPreviewPlacement &intent)
{
    place_preview(intent.placement);
}

// 見せ方の遷移の 1 本（改訂 15・16）。off → full は開くと同じく残りを捨てて即時に頼む（決定
// 3・9）。 side ↔ full は PreviewView を移すだけで、券・錨・追従はそのまま（頼み直さない）。
void EditorController::place_preview(PreviewPlacement placement)
{
    const PreviewPlacement now = state_.preview_placement();
    if (now == placement)
    {
        return;
    }
    switch (placement)
    {
    case PreviewPlacement::off:
        if (now == PreviewPlacement::full)
        {
            leave_full_preview(PreviewPlacement::off);
            return;
        }
        state_ = state_.without_split();
        return;
    case PreviewPlacement::side:
        if (now == PreviewPlacement::full)
        {
            leave_full_preview(PreviewPlacement::side);
            return;
        }
        accept(TogglePreview{});
        return;
    case PreviewPlacement::full:
        break;
    }
    if (now == PreviewPlacement::side)
    {
        state_ = state_.with_full_preview(PreviewView{});
        return;
    }
    static_cast<void>(ports_.markdown.collect());
    settle_follow();
    state_ = state_.with_full_preview(PreviewView{});
    request_preview(true);
}

// 全面をやめる（改訂 16・17）。本文の見える位置を錨の行に揃える（キャレットは動かさない）。
void EditorController::leave_full_preview(PreviewPlacement placement)
{
    const PreviewView *full = state_.full_preview();
    if (full == nullptr)
    {
        return;
    }
    const core::TextBuffer &text = state_.text();
    const ScrollState scroll = settled_scroll();
    const core::Offset anchor =
        full->follows ? text.line_start(scroll.first_visible) : full->anchor;
    const core::LineNumber line = text.position_of(anchor).line;
    preview_vim_g_ = false;
    state_ = state_.without_full_preview(placement);
    state_ = state_.with_scroll(ScrollState{line, scroll.visible_lines, 0, scroll.horizontal});
    clamp_scroll();
}

// `:preview full|side|off` は見せ方を置き、どの形も今の見せ方を 1 行で知らせる（改訂 16）。
void EditorController::answer_preview(core::ExPreviewRequest request)
{
    switch (request)
    {
    case core::ExPreviewRequest::full:
        place_preview(PreviewPlacement::full);
        break;
    case core::ExPreviewRequest::side:
        place_preview(PreviewPlacement::side);
        break;
    case core::ExPreviewRequest::off:
        place_preview(PreviewPlacement::off);
        break;
    case core::ExPreviewRequest::show:
        break;
    }
    std::string_view shown = "preview=off";
    switch (state_.preview_placement())
    {
    case PreviewPlacement::off:
        break;
    case PreviewPlacement::side:
        shown = "preview=side";
        break;
    case PreviewPlacement::full:
        shown = "preview=full";
        break;
    }
    state_ = state_.with_command_message(core::DisplayText::parse(shown).value());
}

// 全面の間の振り分けの入口（改訂 17）。入力行（`:`・検索・Ctrl+P の面）とエクスプローラーの焦点の
// 間は今までどおり写す。Vim の `g` の待ちは次の鍵だけが受ける（合図では消えない）。
bool EditorController::route_full_preview(const EditorIntent &intent)
{
    if (state_.full_preview() == nullptr || state_.command_input().has_value() ||
        explorer_focused())
    {
        return false;
    }
    const auto *press = std::get_if<VimKeyPress>(&intent);
    if (preview_vim_g_ && press != nullptr)
    {
        preview_vim_g_ = false;
        finish_vim_g(*press);
        return true;
    }
    const auto route = preview_full_route(intent, state_.mode(), state_.vim().mode);
    if (route == PreviewFullRoute::pass)
    {
        return false;
    }
    run_full_preview_route(route, intent);
    return true;
}

// `g` の次の鍵（改訂 17）。`gg` は先頭、`gt` `gT` はタブの行き先（engine の道・帯の位置の順・
// ADR 0057）、ほかは本文に届かない。
void EditorController::finish_vim_g(const VimKeyPress &press)
{
    const auto *character = std::get_if<core::VimCharacter>(&press.key);
    const char32_t code = character == nullptr ? U'\0' : character->code;
    if (code == U'g')
    {
        scroll_preview(-static_cast<std::int64_t>(state_.text().line_count()));
        return;
    }
    if (code == U't' || code == U'T')
    {
        accept(VimKeyPress{core::VimKey{core::VimCharacter{U'g'}}});
        accept(press);
        return;
    }
    state_ =
        state_.with_command_message(core::DisplayText::parse(preview_full_refused_message).value());
}

void EditorController::run_full_preview_route(PreviewFullRoute route, const EditorIntent &intent)
{
    const auto page =
        static_cast<std::int64_t>(std::max<std::size_t>(state_.scroll().visible_lines, 1));
    const auto lines = static_cast<std::int64_t>(state_.text().line_count());
    switch (route)
    {
    case PreviewFullRoute::pass:
    case PreviewFullRoute::ignore:
        return;
    case PreviewFullRoute::select:
        select_preview(intent);
        return;
    case PreviewFullRoute::select_all:
        select_all_preview();
        return;
    case PreviewFullRoute::copy:
        copy_preview_selection();
        return;
    case PreviewFullRoute::leave:
        leave_full_preview(state_.full_preview_return());
        return;
    case PreviewFullRoute::line_down:
        scroll_preview(1);
        return;
    case PreviewFullRoute::line_up:
        scroll_preview(-1);
        return;
    case PreviewFullRoute::half_down:
        scroll_preview(std::max<std::int64_t>(page / 2, 1));
        return;
    case PreviewFullRoute::half_up:
        scroll_preview(-std::max<std::int64_t>(page / 2, 1));
        return;
    case PreviewFullRoute::page_down:
        scroll_preview(page);
        return;
    case PreviewFullRoute::page_up:
        scroll_preview(-page);
        return;
    case PreviewFullRoute::top:
        scroll_preview(-lines);
        return;
    case PreviewFullRoute::bottom:
        scroll_preview(lines);
        return;
    case PreviewFullRoute::wheel:
        scroll_preview(wheel_lines(intent));
        return;
    case PreviewFullRoute::vim_g:
        preview_vim_g_ = true;
        return;
    case PreviewFullRoute::task:
        toggle_caret_task();
        return;
    case PreviewFullRoute::refuse:
        state_ = state_.with_command_message(
            core::DisplayText::parse(preview_full_refused_message).value());
        return;
    }
    std::unreachable();
}

// 錨を本文の行で動かし、追従をやめる（決定 7）。側の ScrollPane と全面の鍵が共用する。
void EditorController::scroll_preview(std::int64_t lines)
{
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    const core::TextBuffer &text = state_.text();
    const core::LineNumber top = settled_scroll().first_visible;
    const core::Offset base = view.follows ? text.line_start(top) : view.anchor;
    const auto line = static_cast<std::int64_t>(text.position_of(base).line.value);
    const auto last = static_cast<std::int64_t>(text.line_count());
    const auto moved = std::clamp(line + lines, std::int64_t{1}, last);
    view.anchor = text.line_start(core::LineNumber{static_cast<std::size_t>(moved)});
    view.follows = false;
    state_ = state_.with_shown_preview(std::move(view));
    preview_body_top_ = top;
}

// 全面の通常モードの Space（追補 25）。選択が無く、キャレットの行にタスクの項があれば、その印を
// 切り替える（チェックボックスのクリックと同じ道）。無ければ 1 行知らせる。
void EditorController::toggle_caret_task()
{
    const PreviewView *preview = state_.shown_preview();
    const auto selection = state_.selection();
    // 全面の選択がある間の Space も本文に届かない鍵（追補 25 の「選択の無いとき」）。
    const bool selected =
        preview != nullptr && preview->document != nullptr && preview->selection.has_value() &&
        core::preview_selection_range(*preview->document, preview->selection.value()).has_value();
    if (preview == nullptr || preview->document == nullptr || selection.anchor != selection.caret ||
        selected)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse(preview_full_refused_message).value());
        return;
    }
    const core::TextBuffer &text = state_.text();
    const core::LineNumber line = text.position_of(selection.caret).line;
    const core::Offset begin = text.line_start(line);
    const auto &blocks = preview->document->blocks;
    const auto first =
        std::ranges::lower_bound(blocks, begin.value, {}, [](const core::PreviewBlock &block)
                                 { return block.source_begin.value; });
    for (auto at = first; at != blocks.end() && text.position_of(at->source_begin).line == line;
         ++at)
    {
        if (at->task.has_value())
        {
            accept(TogglePreviewTask{static_cast<std::size_t>(std::distance(blocks.begin(), at))});
            return;
        }
    }
    state_ = state_.with_command_message(core::DisplayText::parse(preview_no_task_message).value());
}

// ---------------------------------------------------------------- 全面の選択（ADR 0155 の改訂 18）

// 全面の間は route_full_preview が select_preview へ渡す。ここに来るのは全面でないときだけで、
// 何もしない。
void EditorController::accept(const PointPreview &) {}

// 動く端を動かす選択。届くのは preview_full_route が select と決めた意図だけ（Shift+矢印の
// MoveCaret と全面のマウスの PointPreview）。全部の選択とコピーは別の行き先（select_all・copy）。
void EditorController::select_preview(const EditorIntent &intent)
{
    if (const auto *move = std::get_if<MoveCaret>(&intent); move != nullptr)
    {
        move_preview_selection(move->motion);
        return;
    }
    if (const auto *point = std::get_if<PointPreview>(&intent); point != nullptr)
    {
        point_preview_selection(*point);
    }
}

// 押した点に空の選択を置く・ドラッグで動く端をそこへ（選び始めた端は保つ）。
void EditorController::point_preview_selection(const PointPreview &intent)
{
    const PreviewView *full = state_.full_preview();
    if (full == nullptr || full->document == nullptr)
    {
        return;
    }
    PreviewView view = *full;
    const auto at = core::preview_point_clamped(*view.document, intent.point);
    const bool extend =
        intent.anchoring == core::SelectionAnchoring::extend && view.selection.has_value();
    view.selection = core::preview_selection_of(
        extend ? core::preview_selection_origin(view.selection.value()) : at, at);
    state_ = state_.with_shown_preview(std::move(view));
}

// Shift+矢印。選択が無ければ枠の上端の要素（錨の要素）の頭から選び始める。
void EditorController::move_preview_selection(core::CaretMotion motion)
{
    const PreviewView *full = state_.full_preview();
    if (full == nullptr || full->document == nullptr)
    {
        return;
    }
    PreviewView view = *full;
    const core::MarkdownDocument &document = *view.document;
    const core::Offset anchor =
        view.follows ? state_.text().line_start(settled_scroll().first_visible) : view.anchor;
    const core::PreviewPoint top{core::preview_anchor_block(document, anchor), 0};
    const core::PreviewPoint origin =
        view.selection.has_value() ? core::preview_selection_origin(view.selection.value()) : top;
    const core::PreviewPoint head =
        view.selection.has_value() ? core::preview_selection_head(view.selection.value()) : top;
    view.selection =
        core::preview_selection_of(origin, core::preview_point_moved(document, head, motion));
    state_ = state_.with_shown_preview(std::move(view));
}

void EditorController::select_all_preview()
{
    const PreviewView *full = state_.full_preview();
    if (full == nullptr || full->document == nullptr)
    {
        return;
    }
    PreviewView view = *full;
    view.selection = core::preview_selection_all(*view.document);
    state_ = state_.with_shown_preview(std::move(view));
}

// 見えている文字を文書の改行の形で置く（ADR 0051 の規則・Ctrl+C と同じ）。選択が無ければ何も
// しない。置けなければ clipboard_written が 1 行知らせる（ADR 0134）。
void EditorController::copy_preview_selection()
{
    const PreviewView *full = state_.full_preview();
    if (full == nullptr || full->document == nullptr || !full->selection.has_value())
    {
        return;
    }
    const std::string text = core::preview_selection_text(*full->document, full->selection.value());
    if (text.empty())
    {
        return;
    }
    static_cast<void>(clipboard_written(
        detail::with_document_newlines(text, core::newline_of(state_.line_ending()))));
}
} // namespace nenenib::application
