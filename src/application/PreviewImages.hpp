#pragma once

#include "FilePath.hpp"
#include "ImageOutcome.hpp"
#include "ImagePixels.hpp"
#include "ImageReferenceRefusal.hpp"
#include "ImageRequest.hpp"
#include "MarkdownDocument.hpp"
#include "PreviewImageEntry.hpp"
#include "PreviewImageStatus.hpp"
#include "PreviewImageWindow.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::application
{
// プレビューの文書の画像の状態（施主決定 D79・ADR 0155 の追補 50・51・#1381）。PreviewView が
// std::shared_ptr<const PreviewImages> で持つ値で、変えるときは controller が写してから変える。画像
// の節（MarkdownDocument.images）の本文の順の番号（序数）を、最初の preview_image_limit 個だけ「経
// 路の entry」か「出さない理由」に引く。同じ経路の節は 1 つの entry を共有する。頼むのは「見える範
// 囲」（PreviewImageWindow）の要素に節がある waiting の entry を、錨の要素に近い順に 1 つずつ（頼み
// 中があれば頼まない）。持つ画素の合計は preview_image_held_byte_limit までで、復号の最大の 1 枚
// （image_decoded_byte_limit）が収まるときだけ頼む。収めるために捨てるのは、頼むものより錨から遠い
// loaded だけで、範囲の外→錨から遠い順に捨てる。
class PreviewImages final
{
  public:
    // document の画像の節を base（文書の場所・無題は nullopt）から解く。previous（同じ文書の前の解
    // 析の状態・無ければ nullptr）の同じ経路の entry は、状態をそのまま引き継ぐ（evicted も evicted
    // のまま）。経路の照合は並べた索引で引く（節の数 n に n log n）。
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

    // claim が何かを変えるか（写す前に問う・変えないなら写さない）。idle は controller の口に頼み中
    // が無いこと（文書が替わる前の要求の結果を待つ間も頼まない）。
    [[nodiscard]] bool wants(const core::MarkdownDocument &document, PreviewImageWindow window,
                             bool idle) const;
    // 見える範囲の外の evicted を waiting に戻す。idle で、この状態に requested が無く、範囲の中に
    // waiting か evicted があれば、錨に最も近い 1 つを requested にして要求を返す（収めるために遠い
    // loaded を捨てる）。券は tickets を 1 つ進めた値（controller の数）。頼めなければ nullopt。
    [[nodiscard]] std::optional<ImageRequest> claim(const core::MarkdownDocument &document,
                                                    PreviewImageWindow window, bool idle,
                                                    std::uint64_t &tickets);
    // 結果を待っている entry に置く。待っている券でない結果は捨てて false。画素が ImagePixels の決
    // まりに合わなければ corrupt の失敗として置く。
    [[nodiscard]] bool received(ImageOutcome outcome);

  private:
    // entry ごとの「範囲の中に節があるか」と「錨の要素からの要素の数の差（節のうち最小）」。
    using Reach = std::pair<std::vector<bool>, std::vector<std::size_t>>;
    // 頼む entry と、そのために捨てる entry（捨てる順）。
    using Plan = std::pair<std::optional<std::size_t>, std::vector<std::size_t>>;

    [[nodiscard]] Reach reach(const core::MarkdownDocument &document,
                              PreviewImageWindow window) const;
    [[nodiscard]] Plan plan(const Reach &reach, bool idle) const;
    [[nodiscard]] std::optional<std::size_t> nearest_waiting(const Reach &reach) const;
    [[nodiscard]] std::vector<std::size_t> farther_loaded(const Reach &reach,
                                                          std::size_t nearest) const;
    void release_outside(const Reach &reach);
    void drop(const Reach &reach, const std::vector<std::size_t> &indices);
    [[nodiscard]] std::optional<std::size_t> entry_of(std::size_t ordinal) const;

    std::vector<PreviewImageEntry> entries_;
    std::vector<std::expected<std::size_t, core::ImageReferenceRefusal>> slots_;
    std::size_t held_bytes_ = 0;
};
} // namespace nenenib::application
