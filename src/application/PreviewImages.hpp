#pragma once

#include "FilePath.hpp"
#include "ImageOutcome.hpp"
#include "ImagePixels.hpp"
#include "ImageReferenceRefusal.hpp"
#include "ImageRequest.hpp"
#include "MarkdownDocument.hpp"
#include "PreviewImageEntry.hpp"
#include "PreviewImageStatus.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::application
{
// プレビューの文書の画像の状態（施主決定 D79・ADR 0155 の追補 50・51・#1381）。PreviewView が
// std::shared_ptr<const PreviewImages> で持つ値で、変えるときは controller が写してから変える。画像
// の節（MarkdownDocument.images）の本文の順の番号（序数）を、最初の preview_image_limit 個だけ「経
// 路の entry」か「出さない理由」に引く。同じ経路の節は 1 つの entry を共有する。頼むのは「見える範
// 囲」の要素 [first_block, end_block) に節がある waiting の entry だけで、持つ画素の合計は
// preview_image_held_byte_limit まで（超えたら読めた順の古いものを evicted にする）。
class PreviewImages final
{
  public:
    // document の画像の節を base（文書の場所・無題は nullopt）から解く。previous（同じ文書の前の解
    // 析の状態・無ければ nullptr）の同じ経路の entry は、requested・loaded・failed をそのまま引き継
    // ぐ（evicted は waiting に戻す）。
    [[nodiscard]] static PreviewImages of(const core::MarkdownDocument &document,
                                          const std::optional<core::FilePath> &base,
                                          const PreviewImages *previous);

    // 状態を引く節の数（画像の節の数と preview_image_limit の小さい方）。
    [[nodiscard]] std::size_t count() const noexcept;
    // 序数の画像の状態。出さない参照と、count 以上の序数は nullopt。
    [[nodiscard]] std::optional<PreviewImageStatus> status_of(std::size_t ordinal) const;
    // 序数の画像を出さない理由（出す参照と count 以上の序数は nullopt）。
    [[nodiscard]] std::optional<core::ImageReferenceRefusal> refusal_of(std::size_t ordinal) const;
    // 序数の画像の経路（出さない参照と count 以上の序数は nullptr）。
    [[nodiscard]] const core::FilePath *path_of(std::size_t ordinal) const;
    // 序数の画像の画素。loaded のときだけ値があり、ほかは null（「[画像: alt]」のまま描く）。
    [[nodiscard]] std::shared_ptr<const ImagePixels> pixels_of(std::size_t ordinal) const;
    // 持っている画素のバイト数の合計。
    [[nodiscard]] std::size_t held_bytes() const noexcept;

    // claim が何かを変えるか（写す前に問う・変えないなら写さない）。
    [[nodiscard]] bool wants(const core::MarkdownDocument &document, std::size_t first_block,
                             std::size_t end_block) const;
    // 見える範囲の外の evicted を waiting に戻し、範囲の中の waiting を requested にして要求を返
    // す。券は tickets を 1 つずつ進めた値（controller の数）。要求は entry の順（本文の順）。
    [[nodiscard]] std::vector<ImageRequest> claim(const core::MarkdownDocument &document,
                                                  std::size_t first_block, std::size_t end_block,
                                                  std::uint64_t &tickets);
    // 結果を待っている entry に置く。待っている券でない結果は捨てて false。画素が ImagePixels の決
    // まりに合わなければ corrupt の失敗として置く。読めたら持つ量の上限まで古いものを捨てる。
    [[nodiscard]] bool received(ImageOutcome outcome);

  private:
    [[nodiscard]] std::vector<bool> windowed(const core::MarkdownDocument &document,
                                             std::size_t first_block, std::size_t end_block) const;
    [[nodiscard]] std::optional<std::size_t> entry_of(std::size_t ordinal) const;
    void loaded(PreviewImageEntry &entry, std::shared_ptr<const ImagePixels> pixels);
    void add_slot(std::expected<core::FilePath, core::ImageReferenceRefusal> path,
                  const std::vector<PreviewImageEntry> *earlier);
    [[nodiscard]] PreviewImageEntry *oldest_loaded(const PreviewImageEntry &kept);
    void evict_beyond_limit(const PreviewImageEntry &kept);

    std::vector<PreviewImageEntry> entries_;
    std::vector<std::expected<std::size_t, core::ImageReferenceRefusal>> slots_;
    std::size_t held_bytes_ = 0;
    std::uint64_t loads_ = 0;
};
} // namespace nenenib::application
