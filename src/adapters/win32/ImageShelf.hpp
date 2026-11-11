#pragma once

#include "ImageOutcome.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// 画像を読む仕事と UI スレッドが共有する棚
// （施主決定 D79・ADR 0155 の追補 54・#1412・MarkdownShelf と同じ形）。
// 要求ごとに結果を必ず 1 つ返すので、たまりは置き換えず全部積む。
// 積むときに記憶を取らないように、要求を受けた UI スレッドが reserve で
// 1 つ分の席を先に取り、ワーカーの deliver は取ってある席に置く
// （復号で記憶が足りなくなっても、insufficient_memory の結果を置ける）。
// たまりと合図は 1 つのロックで守る。合図はたまりが空から 1 つ以上になった
// ときだけ、ロックの外で呼ぶ（合図の写しは shared_ptr で、写すときに記憶を
// 取らない）。仕事が shared_ptr で持つので、adapter が先に壊れても残る。
class ImageShelf final
{
  public:
    ImageShelf() = default;

    void bind(std::function<void()> arrived);
    // 要求 1 つ分の席を取る（UI スレッド・要求を仕事に載せる前）。
    void reserve();
    // 以後の結果を受けず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    // 閉じたか（読み始める前に問う・閉じていれば読まずに戻る）。
    [[nodiscard]] bool closed() const;
    // reserve で取った席に結果を置く。閉じた後は置かない。記憶を取らない。
    void deliver(application::ImageOutcome outcome) noexcept;
    [[nodiscard]] std::vector<application::ImageOutcome> take();

  private:
    mutable std::mutex mutex_;
    // outcomes_ の capacity は「積んだ数 + reserved_」以上を保つ。
    std::vector<application::ImageOutcome> outcomes_;
    std::size_t reserved_ = 0;
    bool closed_ = false;
    std::shared_ptr<const std::function<void()>> arrived_;
};
} // namespace nenenib::adapters::win32
