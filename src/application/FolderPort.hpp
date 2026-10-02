#pragma once

#include "FolderBatch.hpp"
#include "FolderRequest.hpp"

#include <vector>

namespace nenenib::application
{
// 同じフォルダを裏で読む口（ADR 0062 の決定 6・ADR 0004 の仕事の種類ごとの port）。
// スレッドは adapters に閉じる。
// list は頼んですぐ戻る。前の要求はやめられ、新しい要求だけが batch を出す。
// collect は読めた分を全部受け取る（無ければ空）。UI スレッドだけが呼ぶ。
class FolderPort
{
  public:
    FolderPort() = default;
    virtual ~FolderPort() = default;
    FolderPort(const FolderPort &) = delete;
    FolderPort(FolderPort &&) = delete;
    FolderPort &operator=(const FolderPort &) = delete;
    FolderPort &operator=(FolderPort &&) = delete;

    virtual void list(FolderRequest request) = 0;
    [[nodiscard]] virtual std::vector<FolderBatch> collect() = 0;
};
} // namespace nenenib::application
