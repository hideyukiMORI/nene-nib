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
//
// 段 2 への申し送り（ADR 0155 の追補 54）:
// - 開いた後に GetFileType が FILE_TYPE_DISK でなければ unreadable（core は予約のデバイス名 `NUL`
//   `CON` `COM1` `CONIN$` を経路として通すので、名前でなく OS の判定で断る）。
// - 経路は文書のフォルダの外も指せる（`../../x.png`・`/a.png`・ドライブの絶対パス）。どれもローカル
//   のファイルで、D79 が断るのはネット（http / https）だけなので許す。
// - 画素は ImagePixels の大きさで 1 回だけ確保し、CopyPixels の stride は size.width * 4（詰め物な
//   し）。
class Win32ImageAdapter final : public application::ImagePort
{
  public:
    void request(application::ImageRequest request) override;
    [[nodiscard]] std::vector<application::ImageOutcome> collect() override;
};
} // namespace nenenib::adapters::win32
