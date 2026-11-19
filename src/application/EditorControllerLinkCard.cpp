// プレビューのリンクの札（施主決定 D105・D106・#1869 の段 L2）。押した点と離した点が同じリンクに
// 当たれば札を開き（宛先の種類は core::link_destination の 1 本）、札の鍵とボタンで開く・写す・
// 閉じる。web と mail は LinkOpenPort（OS の既定の道・adapters がワーカーで渡す）、document は
// open_listed の 1 本（Nib の新しいタブ）。札の状態は EditorState が持ち、frame に載るのは見せて
// いる結果の同じ要素に同じリンクがある間だけ。当たりの計算（押した点 → 要素とバイト位置）は ui、
// リンクの判定はここ。同じ文書の `#見出し` は札を開かずに跳ぶ（段 L3・jump_to_heading）。
#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "ActOnLinkCard.hpp"
#include "AutosaveId.hpp"
#include "ClickPreviewLink.hpp"
#include "Column.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "HeadingSlug.hpp"
#include "LineNumber.hpp"
#include "LinkCard.hpp"
#include "LinkCardAction.hpp"
#include "LinkCardView.hpp"
#include "LinkDestination.hpp"
#include "LinkKind.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "OpenedAs.hpp"
#include "PlaceCaret.hpp"
#include "PreviewBlock.hpp"
#include "PreviewView.hpp"
#include "SelectionAnchoring.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "VimMarkEdits.hpp"
#include "VimStep.hpp"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// 札を開いたまま受ける意図。
[[nodiscard]] bool is_link_card_intent(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<ClickPreviewLink>(intent) ||
           std::holds_alternative<ActOnLinkCard>(intent);
}

// 札で開ける種類か（heading は段 L3・refused は開かない）。
[[nodiscard]] bool openable(core::LinkKind kind) noexcept
{
    switch (kind)
    {
    case core::LinkKind::web:
    case core::LinkKind::mail:
    case core::LinkKind::document:
        return true;
    case core::LinkKind::heading:
    case core::LinkKind::refused:
        return false;
    }
    std::unreachable();
}
} // namespace

bool EditorController::link_card_active() const
{
    return shown_link_card().has_value();
}

std::optional<LinkCardView> EditorController::shown_link_card() const
{
    const auto &card = state_.link_card();
    const PreviewView *preview = state_.shown_preview();
    if (!card.has_value() || !(card.value().tab == active_tab_id()) || preview == nullptr ||
        preview->document == nullptr || card.value().block >= preview->document->blocks.size())
    {
        return std::nullopt;
    }
    const auto &links = preview->document->blocks.at(card.value().block).links;
    return std::ranges::find(links, card.value().link) == links.end() ? std::nullopt : card;
}

AutosaveId EditorController::active_tab_id() const
{
    return state_.autosave_ids().at(state_.active_tab());
}

void EditorController::close_link_card_before(const EditorIntent &intent)
{
    if (state_.link_card().has_value() && !is_link_card_intent(intent) &&
        !detail::not_user_operation(intent))
    {
        state_ = state_.with_link_card(std::nullopt);
    }
}

// アクティブな文書が札を開いたタブでなくなったら札を消す（合図の道でタブが替わったときも・#1876 の
// レビュー R1）。古い文書のフォルダで解いた宛先を、別のタブで開かない。
void EditorController::drop_stale_link_card()
{
    const auto &card = state_.link_card();
    if (card.has_value() && !(card.value().tab == active_tab_id()))
    {
        state_ = state_.with_link_card(std::nullopt);
    }
}

// 押した点と離した点が同じ要素の同じリンクなら札を開く（前の札は閉じる）。ほかは閉じるだけ。
void EditorController::accept(const ClickPreviewLink &intent)
{
    state_ = state_.with_link_card(std::nullopt);
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr || preview->document == nullptr ||
        intent.pressed.block != intent.released.block ||
        intent.pressed.block >= preview->document->blocks.size())
    {
        return;
    }
    const core::PreviewBlock &block = preview->document->blocks.at(intent.pressed.block);
    const auto pressed = core::preview_link_at(block, intent.pressed.offset);
    if (!pressed.has_value() || pressed != core::preview_link_at(block, intent.released.offset))
    {
        return;
    }
    const core::PreviewLink &link = block.links.at(pressed.value());
    auto destination = core::link_destination(link.target, state_.document().path);
    // 同じ文書の見出しは札を開かずに跳ぶ（段 L3・確かめない）。
    if (destination.kind == core::LinkKind::heading)
    {
        jump_to_heading(destination.text);
        return;
    }
    state_ = state_.with_link_card(
        LinkCardView{intent.pressed.block, link, std::move(destination), active_tab_id()});
}

void EditorController::accept(const ActOnLinkCard &intent)
{
    const auto card = shown_link_card();
    if (!card.has_value())
    {
        state_ = state_.with_link_card(std::nullopt);
        return;
    }
    const bool refused = !openable(card.value().destination.kind);
    switch (intent.action)
    {
    case LinkCardAction::open:
        if (!refused)
        {
            open_link_card(card.value());
        }
        return;
    case LinkCardAction::primary:
        if (refused)
        {
            copy_link_card(card.value());
            return;
        }
        open_link_card(card.value());
        return;
    case LinkCardAction::copy:
        copy_link_card(card.value());
        return;
    case LinkCardAction::close:
    case LinkCardAction::secondary:
        state_ = state_.with_link_card(std::nullopt);
        return;
    }
    std::unreachable();
}

// 開いたら札を閉じる。web と mail は綴りのまま port へ（失敗は合図の後の 1 行）、document は
// 一覧から開くのと同じ open_listed（失敗は 1 行の知らせ）。
void EditorController::open_link_card(const LinkCardView &card)
{
    state_ = state_.with_link_card(std::nullopt);
    switch (card.destination.kind)
    {
    case core::LinkKind::web:
    case core::LinkKind::mail:
        ports_.links.open(card.destination.text);
        ++link_opens_pending_;
        return;
    case core::LinkKind::document:
        open_link_document(card.destination.text);
        return;
    case core::LinkKind::heading:
    case core::LinkKind::refused:
        return;
    }
    std::unreachable();
}

void EditorController::open_link_document(std::string_view path)
{
    const auto parsed = core::FilePath::parse(path);
    if (!parsed.has_value())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_link_open_failed));
        return;
    }
    open_listed(parsed.value(), std::nullopt, OpenedAs::pinned);
}

// 名札の列（解析の 1 回で作った MarkdownDocument.headings）で見出しを引き、その要素の元の行
// （source_begin・見せている結果が古い本文のものなら本文の長さに寄せる）の頭へキャレットを置く。
// 前の位置は全文検索の着地と同じ `m'` の 1 本で `''` とジャンプの列に置く（Vim の `:tag` と同じく
// 跳ぶ命令と数える）。プレビューは側も全面も、錨をその見出しの行へ置いて追従をやめる（プレビューの
// 送りの絶対の行き先と同じ形・anchor_preview_at）。本文が次に送られると側のプレビューは追従に戻る。
void EditorController::jump_to_heading(std::string_view fragment)
{
    const PreviewView *preview = state_.shown_preview();
    if (preview == nullptr || preview->document == nullptr)
    {
        return;
    }
    const auto document = preview->document;
    const auto block = core::heading_block_of(document->headings, fragment);
    if (!block.has_value())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_heading_not_found));
        return;
    }
    const core::TextBuffer &text = state_.text();
    const core::Offset begin{
        std::min(document->blocks.at(block.value()).source_begin.value, text.size_bytes())};
    const core::LineNumber line = text.position_of(begin).line;
    const auto from = core::vim_mark_position_of(text, state_.selection().caret);
    auto marks = core::vim_mark_set(state_.vim_marks(), U'\'', from);
    if (marks.has_value())
    {
        state_ = state_.with_vim_marks(std::move(marks).value());
    }
    // 待っているオペレータと回数は捨てる（見出しの行がオペレータの動きにならない・メニューの
    // 実行と同じ vim_operator_cleared の 1 本・VISUAL は保つ）。
    state_ = state_.with_vim(core::vim_operator_cleared(state_.vim()));
    accept(
        PlaceCaret{core::TextPosition{line, core::Column{1}}, core::SelectionAnchoring::collapse});
    anchor_preview_at(line);
}

// 宛先の綴りを写す（失敗は clipboard_written の 1 行・札は開いたまま）。写せたら札を閉じて 1 行
// 知らせる。
void EditorController::copy_link_card(const LinkCardView &card)
{
    if (!clipboard_written(card.destination.text))
    {
        return;
    }
    state_ = state_.with_link_card(std::nullopt);
    state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_link_copied));
}

void EditorController::collect_link_opens()
{
    // 合図の道の読み（collect_reads）の後に呼ばれるので、裏の読みで替わったタブの札もここで消す。
    drop_stale_link_card();
    if (link_opens_pending_ == 0)
    {
        return;
    }
    const auto results = ports_.links.collect();
    link_opens_pending_ -= std::min(link_opens_pending_, results.size());
    for (const auto &result : results)
    {
        if (!result.has_value())
        {
            state_ =
                state_.with_command_message(ui_notice(core::UiMessage::notice_link_open_failed));
        }
    }
}
} // namespace nenenib::application
