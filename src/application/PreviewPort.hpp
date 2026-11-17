#pragma once

#include "PreviewBatch.hpp"
#include "PreviewRequest.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 開いていないファイルのプレビューの窓を裏で読む口（ADR 0189 の決定 4・FolderPort と同じ形・
// ADR 0004 の仕事の種類ごとの port）。スレッドとファイルは adapters に閉じる。
// request は頼んですぐ戻る。新しい要求は前の要求をやめる。cancel は券の要求をやめる（届いていない
// batch は出さなくてよい）。完了の知らせは WorkCompleted の合図で、collect が届いた分を全部受け取る
// （無ければ空）。UI スレッドだけが呼ぶ。
class PreviewPort
{
  public:
    PreviewPort() = default;
    virtual ~PreviewPort() = default;
    PreviewPort(const PreviewPort &) = delete;
    PreviewPort(PreviewPort &&) = delete;
    PreviewPort &operator=(const PreviewPort &) = delete;
    PreviewPort &operator=(PreviewPort &&) = delete;

    virtual void request(PreviewRequest request) = 0;
    virtual void cancel(std::uint64_t ticket) = 0;
    [[nodiscard]] virtual std::vector<PreviewBatch> collect() = 0;
};
} // namespace nenenib::application
