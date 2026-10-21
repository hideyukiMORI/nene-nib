#pragma once

#include "MarkdownDocument.hpp"
#include "MarkdownStop.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewStyle.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// md4c の enter / leave の積みを平らな leaf ブロックの列に畳む
// （ADR 0151 の決定 7・8）。md4c の型を知らない（md4c.h を読むのは
// MarkdownParse.cpp だけ・決定 5）。積み（引用の深さ・リストの番号・
// 開いた装飾）はここの中だけに持ち、leaf ごとに 1 要素を出す。
// 項目（LI）の直下の文字は暗黙の leaf として list_item に、
// 項目の最初の段落も list_item に畳み、項目の記号はその最初の要素が持つ。
class MarkdownBuilder final
{
  public:
    // input は本文の全体。text の source がこの中を指すときだけ位置に使う。
    MarkdownBuilder(std::string_view input, const MarkdownStop &stop) noexcept;

    // 述語に問う。一度 true を聞いたら以後は問わずに true を返し続ける（打ち切りを取り消さない）。
    [[nodiscard]] bool should_stop();
    [[nodiscard]] bool stopped() const noexcept;
    // コールバックか述語が std::bad_alloc を投げたことを覚える（#817）。
    void exhaust() noexcept;
    [[nodiscard]] bool exhausted() const noexcept;

    void enter_quote();
    void leave_quote();
    // first は番号つきの最初の番号。箇条書きは nullopt。
    void enter_list(std::optional<std::uint64_t> first);
    void leave_list();
    void enter_item();
    void leave_item();
    void rule();
    void open_leaf(PreviewBlockKind kind, std::uint8_t level);
    void close_leaf();
    // style が nullopt の装飾（画像など）は積みの対だけを保つ。
    void enter_span(std::optional<PreviewStyle> style);
    void leave_span();
    // shown を要素の文字に足す。source が本文の中を指せば、要素の最初の位置と直前の終わりに使う。
    void text(std::string_view shown, std::string_view source);

    [[nodiscard]] MarkdownDocument finished() &&;

  private:
    [[nodiscard]] std::optional<std::size_t> located(std::string_view source) const;
    [[nodiscard]] std::size_t unlocated_begin() const noexcept;
    [[nodiscard]] PreviewBlock started(PreviewBlockKind kind, std::uint8_t level);
    [[nodiscard]] PreviewBlock &opened(PreviewBlockKind kind, std::uint8_t level);
    PreviewBlock &ensure_leaf();
    void close_implicit();
    void flush_marker();

    std::string_view input_;
    const MarkdownStop *stop_;
    std::vector<PreviewBlock> blocks_;
    std::optional<PreviewBlock> leaf_;
    std::optional<std::size_t> leaf_begin_;
    bool implicit_leaf_ = false;
    std::optional<std::string> marker_;
    std::size_t quote_depth_ = 0;
    std::vector<std::optional<std::uint64_t>> lists_;
    std::vector<std::optional<std::size_t>> spans_;
    std::optional<std::size_t> last_end_;
    bool stopped_ = false;
    bool exhausted_ = false;
};
} // namespace nenenib::core
