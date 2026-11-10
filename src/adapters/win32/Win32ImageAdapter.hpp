#pragma once

#include "ImageOutcome.hpp"
#include "ImagePort.hpp"
#include "ImageRequest.hpp"

#include <vector>

namespace nenenib::adapters::win32
{
// Markdown プレビューの画像を読む口の実装（施主決定 D79・ADR 0155 の追補 53・#1381）。いまは口だけ
// で、ファイルの読み・先頭のバイト列の判定・WIC の復号と縮小・Win32Worker と棚と合図は段 2 が入れ
// る。それまでは要求を受け取って捨て、結果を 1 つも返さない（画像は「[画像: alt]」の文字のまま・製
// 品の振る舞いは段 1 の前と同じ）。ファイルにも OS の画像の API にもまだ触れない。
class Win32ImageAdapter final : public application::ImagePort
{
  public:
    void request(application::ImageRequest request) override;
    [[nodiscard]] std::vector<application::ImageOutcome> collect() override;
};
} // namespace nenenib::adapters::win32
