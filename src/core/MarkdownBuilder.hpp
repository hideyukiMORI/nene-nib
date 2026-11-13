#pragma once

#include "DisplayLanguage.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownImage.hpp"
#include "MarkdownStop.hpp"
#include "MarkdownTask.hpp"
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
    MarkdownBuilder(std::string_view input, const MarkdownStop &stop,
                    DisplayLanguage language) noexcept;

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
    // task はタスクリストの項目のチェックボックス（ADR 0155 の追補 22・ほかは nullopt）。項目の
    // 記号と同じく、項目の最初の要素が持つ。
    void enter_item(std::optional<MarkdownTask> task);
    void leave_item();
    void rule();
    void open_leaf(PreviewBlockKind kind, std::uint8_t level);
    void close_leaf();
    // style が nullopt の装飾（打ち消しなど）は積みの対だけを保つ。
    void enter_span(std::optional<PreviewStyle> style);
    // 画像の装飾を開き、画像の節を覚える（ADR 0155 の追補 47）。source と title は実体参照を解いた
    // md4c の属性。閉じる leave_span が alt と範囲の終わりを書く。
    void enter_image(std::string source, std::string title);
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
    // 開いている画像の節のいちばん内側を、要素の text の今の終わりで閉じる。closed は閉じの「]」を
    // 足した後か。
    void finish_image(const PreviewBlock &block, bool closed);

    std::string_view input_;
    const MarkdownStop *stop_;
    // 画像の代わりの文字の枠の前と後ろ（表の文 preview_image_alt を置き場で割った静的な文）。
    std::string_view image_opening_;
    std::string_view image_closing_;
    std::vector<PreviewBlock> blocks_;
    std::optional<PreviewBlock> leaf_;
    std::optional<std::size_t> leaf_begin_;
    bool implicit_leaf_ = false;
    std::optional<std::string> marker_;
    std::optional<MarkdownTask> task_;
    std::size_t quote_depth_ = 0;
    std::vector<std::optional<std::uint64_t>> lists_;
    std::vector<std::optional<std::size_t>> spans_;
    std::vector<MarkdownImage> images_;
    // 開いている画像の節の images_ の番号（入れ子は外側が先）。
    std::vector<std::size_t> open_images_;
    std::optional<std::size_t> last_end_;
    bool stopped_ = false;
    bool exhausted_ = false;
};
} // namespace nenenib::core
