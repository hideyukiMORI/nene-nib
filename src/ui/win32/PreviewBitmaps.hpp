#pragma once

#include "ImagePixels.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1_1.h>
#include <memory>
#include <tuple>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// プレビューの読めた画像の ID2D1Bitmap1 の保持（施主決定 D79・ADR 0155 の追補 63・#1439）。
// PreviewPane が 1 つ持つ。bitmap は application の画素（ImagePixels・32bppPBGRA・
// 詰め物なし）の写しで、画素の同一性（指す先）で引く。画素は弱い参照でだけ覚え、ui は画素を
// 生かし続けない（application が捨てた画素・閉じたタブ・閉じたプレビューの画素は、次の begin
// で bitmap ごと捨てる）。保持の合計は core::preview_image_held_byte_limit（32 MiB）までで、
// 超えたら今の描画で使わないものを古い順に捨てる。描く先（device context）が替われば全部
// 捨てる（device lost の作り直しは renderer ごと作り直すので、ここも新しくなる）。
class PreviewBitmaps final
{
  public:
    // 1 回の描画の始め。context が前と違えば全部捨て、画素が放たれたものを捨てる。
    void begin(const ID2D1DeviceContext &context);
    // pixels の bitmap を今の描画で使う（無ければ context に作る）。作れなければ失敗の HRESULT。
    [[nodiscard]] HRESULT hold(ID2D1DeviceContext &context,
                               const std::shared_ptr<const application::ImagePixels> &pixels);
    // pixels の bitmap（無ければ nullptr）。画素が生きている間だけ答える。
    [[nodiscard]] ID2D1Bitmap1 *find(const application::ImagePixels *pixels) const;
    // 全部捨てる（プレビューを描かない frame で renderer が呼ぶ）。
    void release() noexcept;
    // 持っている bitmap の数・画素のバイト数の合計・作った数（試験が数える）。
    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] std::size_t bytes() const noexcept;
    [[nodiscard]] std::size_t created() const noexcept;

  private:
    // 画素の弱い参照・画素の指す先（引く鍵）・bitmap・画素のバイト数・最後に使った描画の番号。
    using Entry =
        std::tuple<std::weak_ptr<const application::ImagePixels>, const application::ImagePixels *,
                   Microsoft::WRL::ComPtr<ID2D1Bitmap1>, std::size_t, std::uint64_t>;
    // 合計が上限を超える間、今の描画で使わないものを古い順に捨てる。
    void trim();

    const ID2D1DeviceContext *context_ = nullptr;
    std::vector<Entry> held_;
    std::size_t bytes_ = 0;
    std::size_t created_ = 0;
    std::uint64_t paint_ = 0;
};
} // namespace nenenib::ui::win32
