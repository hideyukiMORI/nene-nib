#include "MarkdownParse.hpp"

#include "MarkdownBuilder.hpp"
#include "MarkdownEntity.hpp"
#include "MarkdownTask.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewStyle.hpp"

// md4c の宣言を読むのはこの 1 ファイルだけ（ADR 0151 の決定 5・eng/conformance.py が守る）。
#include <md4c.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nenenib::core
{
namespace
{
static_assert(std::is_same_v<MD_SIZE, unsigned>);
static_assert(std::is_same_v<MD_CHAR, char>);

// コールバックが打ち切りを伝える値。md4c の内側の関数は MD_CHECK で負の値だけを上へ返し、正の値は
// 捨てて読み続ける（md4c.c の MD_CHECK・#765 の試験で実測）。md4c 自身が返す負の値は -1 だけ
// なので、-1 と重ならない負の値にする。
constexpr int stop_code = -2;
constexpr std::string_view replacement_character = "�";

[[nodiscard]] MarkdownBuilder &builder_of(void *userdata) noexcept
{
    return *static_cast<MarkdownBuilder *>(userdata);
}

[[nodiscard]] std::uint8_t heading_level(const void *detail) noexcept
{
    return static_cast<std::uint8_t>(static_cast<const MD_BLOCK_H_DETAIL *>(detail)->level);
}

// タスクリストの項目のチェックボックス（ADR 0155 の追補 22）。task_mark_offset は本文の
// バイト位置で、`[` と `]` の間の 1 バイト（md4c.h の MD_BLOCK_LI_DETAIL）。
[[nodiscard]] std::optional<MarkdownTask> item_task(const void *detail) noexcept
{
    const auto *item = static_cast<const MD_BLOCK_LI_DETAIL *>(detail);
    if (item->is_task == 0)
    {
        return std::nullopt;
    }
    return MarkdownTask{.checked = item->task_mark != ' ',
                        .mark_offset = static_cast<std::size_t>(item->task_mark_offset)};
}

[[nodiscard]] std::uint64_t list_start(const void *detail) noexcept
{
    return static_cast<const MD_BLOCK_OL_DETAIL *>(detail)->start;
}

// 表（MD_BLOCK_TABLE 系）は flag を立てないので来ない。来たらセルを段落として扱う。
void entered_block(MarkdownBuilder &builder, MD_BLOCKTYPE type, const void *detail)
{
    switch (type)
    {
    case MD_BLOCK_DOC:
    case MD_BLOCK_TABLE:
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
    case MD_BLOCK_TR:
        return;
    case MD_BLOCK_QUOTE:
        builder.enter_quote();
        return;
    case MD_BLOCK_UL:
        builder.enter_list(std::nullopt);
        return;
    case MD_BLOCK_OL:
        builder.enter_list(list_start(detail));
        return;
    case MD_BLOCK_LI:
        builder.enter_item(item_task(detail));
        return;
    case MD_BLOCK_HR:
        builder.rule();
        return;
    case MD_BLOCK_H:
        builder.open_leaf(PreviewBlockKind::heading, heading_level(detail));
        return;
    case MD_BLOCK_CODE:
        builder.open_leaf(PreviewBlockKind::code, 0U);
        return;
    case MD_BLOCK_HTML:
        builder.open_leaf(PreviewBlockKind::html, 0U);
        return;
    case MD_BLOCK_P:
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        builder.open_leaf(PreviewBlockKind::paragraph, 0U);
        return;
    }
}

void left_block(MarkdownBuilder &builder, MD_BLOCKTYPE type)
{
    switch (type)
    {
    case MD_BLOCK_DOC:
    case MD_BLOCK_TABLE:
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
    case MD_BLOCK_TR:
    case MD_BLOCK_HR:
        return;
    case MD_BLOCK_QUOTE:
        builder.leave_quote();
        return;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        builder.leave_list();
        return;
    case MD_BLOCK_LI:
        builder.leave_item();
        return;
    case MD_BLOCK_H:
    case MD_BLOCK_CODE:
    case MD_BLOCK_HTML:
    case MD_BLOCK_P:
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        builder.close_leaf();
        return;
    }
}

// 装飾の種類。画像は代わりの文字の装飾（builder が「[画像: alt]」の枠を足す）。打ち消し・数式・
// wiki リンク・下線は装飾を持たない（flag を立てないので来ない）。
[[nodiscard]] std::optional<PreviewStyle> span_style(MD_SPANTYPE type) noexcept
{
    switch (type)
    {
    case MD_SPAN_EM:
        return PreviewStyle::emphasis;
    case MD_SPAN_STRONG:
        return PreviewStyle::strong;
    case MD_SPAN_CODE:
        return PreviewStyle::code;
    case MD_SPAN_A:
        return PreviewStyle::link;
    case MD_SPAN_IMG:
        return PreviewStyle::image;
    case MD_SPAN_DEL:
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
    case MD_SPAN_WIKILINK:
    case MD_SPAN_U:
        return std::nullopt;
    }
    return std::nullopt;
}

// NORMAL・CODE・HTML は綴りのまま（CODE / HTML の行末と字下げは md4c の固定の文字列で、本文の外を
// 指すので位置には使われない）。ENTITY は表で解く。BR は改行、SOFTBR は空白 1 つ（段落の中の改行は
// 描画が折る）、NULLCHAR は U+FFFD（md4c の勧め）。
void received_text(MarkdownBuilder &builder, MD_TEXTTYPE type, std::string_view piece)
{
    switch (type)
    {
    case MD_TEXT_NORMAL:
    case MD_TEXT_CODE:
    case MD_TEXT_HTML:
    case MD_TEXT_LATEXMATH:
        builder.text(piece, piece);
        return;
    case MD_TEXT_ENTITY:
        builder.text(markdown_entity_text(piece), piece);
        return;
    case MD_TEXT_NULLCHAR:
        builder.text(replacement_character, {});
        return;
    case MD_TEXT_BR:
        builder.text("\n", {});
        return;
    case MD_TEXT_SOFTBR:
        builder.text(" ", {});
        return;
    }
}

// 述語に問い、続けるなら step を行う。コールバックか述語が投げた bad_alloc はここで受けて印を立て、
// 打ち切りの値を返す。例外は md4c の C のフレームを通らず、md4c は自分の確保を解放して戻る（#817・
// ARC-010）。/EHsc は extern "C" の md_parse を投げないものとして扱うので、
// md_parse の外では受けない。
template <class Step> [[nodiscard]] int guarded(MarkdownBuilder &builder, const Step &step) noexcept
{
    try
    {
        if (builder.should_stop())
        {
            return stop_code;
        }
        step();
        return 0;
    }
    catch (const std::bad_alloc &)
    {
        builder.exhaust();
        return stop_code;
    }
}

// md4c は C の呼び出し規約で関数ポインタを呼ぶ。clang-cl の既定と一致するので
// extern "C" は要らない。打ち切りの述語はコールバックのたびに問う（決定 6）。
int on_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    auto &builder = builder_of(userdata);
    return guarded(builder, [&] { entered_block(builder, type, detail); });
}

int on_leave_block(MD_BLOCKTYPE type, void * /*detail*/, void *userdata)
{
    auto &builder = builder_of(userdata);
    return guarded(builder, [&] { left_block(builder, type); });
}

int on_enter_span(MD_SPANTYPE type, void * /*detail*/, void *userdata)
{
    auto &builder = builder_of(userdata);
    return guarded(builder, [&] { builder.enter_span(span_style(type)); });
}

int on_leave_span(MD_SPANTYPE /*type*/, void * /*detail*/, void *userdata)
{
    auto &builder = builder_of(userdata);
    return guarded(builder, [&] { builder.leave_span(); });
}

int on_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    auto &builder = builder_of(userdata);
    return guarded(builder, [&] { received_text(builder, type, std::string_view(text, size)); });
}
} // namespace

bool markdown_size_fits(std::size_t size) noexcept
{
    return size <= std::numeric_limits<MD_SIZE>::max();
}

std::expected<MarkdownDocument, MarkdownFailure> parse_markdown(std::string_view text,
                                                                const MarkdownStop &stop)
{
    if (!markdown_size_fits(text.size()))
    {
        return std::unexpected(MarkdownFailure::too_large);
    }
    MarkdownBuilder builder(text, stop);
    if (text.empty())
    {
        return std::move(builder).finished();
    }
    const MD_PARSER parser{
        .abi_version = 0U,
        .flags = MD_DIALECT_COMMONMARK | MD_FLAG_TASKLISTS,
        .enter_block = on_enter_block,
        .leave_block = on_leave_block,
        .enter_span = on_enter_span,
        .leave_span = on_leave_span,
        .text = on_text,
        .debug_log = nullptr,
        .syntax = nullptr,
    };
    if (md_parse(text.data(), static_cast<MD_SIZE>(text.size()), &parser, &builder) == 0)
    {
        return std::move(builder).finished();
    }
    if (builder.exhausted())
    {
        return std::unexpected(MarkdownFailure::insufficient_memory);
    }
    if (builder.stopped())
    {
        return std::unexpected(MarkdownFailure::stopped);
    }
    return std::unexpected(MarkdownFailure::parser_rejected);
}
} // namespace nenenib::core
