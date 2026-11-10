#pragma once

#include "ImageOutcome.hpp"
#include "ImageRequest.hpp"

#include <vector>

namespace nenenib::application
{
// Markdown プレビューの画像を裏で読む口（施主決定 D79・ADR 0155 の追補 53・#1381・ADR 0004 の仕事の
// 種類ごとの port）。スレッドとファイルと WIC は adapters に閉じる（段 2）。形は FolderPort と
// MarkdownPort と同じで、合図を結ぶ bind は adapter が持つ（port には置かない）。request は頼んです
// ぐ戻る。要求は届いた順に 1 つずつ読み、要求ごとに結果を 1 つ返す（読み終える前に窓が閉じた分は返
// さなくてよい）。上限（core/ImageLimits.hpp）を超える・読めない・知らない形式は失敗の結果で返す。
// collect は届いた結果を全部受け取る（無ければ空）。結果のたまりが空から 1 つになったときに合図し、
// application は WorkCompleted で collect する。UI スレッドだけが呼ぶ。application はプレビューが開
// いている間だけ呼ぶ（起動・開く・切り替え・保存・打鍵の道では呼ばない）。
class ImagePort
{
  public:
    ImagePort() = default;
    virtual ~ImagePort() = default;
    ImagePort(const ImagePort &) = delete;
    ImagePort(ImagePort &&) = delete;
    ImagePort &operator=(const ImagePort &) = delete;
    ImagePort &operator=(ImagePort &&) = delete;

    virtual void request(ImageRequest request) = 0;
    [[nodiscard]] virtual std::vector<ImageOutcome> collect() = 0;
};
} // namespace nenenib::application
