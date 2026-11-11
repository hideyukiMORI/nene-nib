#pragma once

#include "ImageOutcome.hpp"
#include "ImagePort.hpp"
#include "ImageRequest.hpp"
#include "ImageShelf.hpp"
#include "Win32Worker.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace nenenib::adapters::win32
{
// Markdown プレビューの画像を読む口の実装
// （施主決定 D79・ADR 0155 の追補 53・54・#1412）。
// 読みはワーカーを借りて走る（folders・markdown と同じ・スレッドは足さない）。
// 要求は届いた順に 1 つずつ:
// - ファイルを読む（ディスクのファイルだけ・16 MiB まで・image_file_bytes）。
// - 先頭のバイト列で形式を決める（image_format_of・unknown は unsupported）。
// - WIC で復号して長辺 2048 に縮める（decoded_image）。
// - 結果を棚に置いて合図する。
// 要求ごとに結果を必ず 1 つ返す。読めない・壊れている・上限超え・記憶が足りないも
// 失敗の結果で、仕事を載せられないときは request の中で insufficient_memory を置く。
// 返さないのは、adapter が壊れた後（窓が閉じる途中）に読み終えた分だけ。
// 「届いた」の合図は bind で後から受け取る。HWND も窓メッセージの番号も知らない。
class Win32ImageAdapter final : public application::ImagePort
{
  public:
    explicit Win32ImageAdapter(Win32Worker &worker);
    ~Win32ImageAdapter() override;
    Win32ImageAdapter(const Win32ImageAdapter &) = delete;
    Win32ImageAdapter(Win32ImageAdapter &&) = delete;
    Win32ImageAdapter &operator=(const Win32ImageAdapter &) = delete;
    Win32ImageAdapter &operator=(Win32ImageAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void request(application::ImageRequest request) override;
    [[nodiscard]] std::vector<application::ImageOutcome> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<ImageShelf> shelf_;
};
} // namespace nenenib::adapters::win32
