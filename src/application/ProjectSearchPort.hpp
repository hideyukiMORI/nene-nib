#pragma once

#include "ProjectSearchBatch.hpp"
#include "ProjectSearchRequest.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// プロジェクトの全文検索を裏で行う口（ADR 0167 の決定 4・FolderPort と同じ形・ADR 0004 の仕事の
// 種類ごとの port）。スレッドとファイルは adapters に閉じる。
// request は頼んですぐ戻る。新しい要求は前の要求をやめる。cancel は券の要求をやめる（残りの仕事は
// 何もせずに戻る・届いていない batch は出さなくてよい）。完了と途中の知らせは WorkCompleted の
// 合図で、collect が届いた分を全部受け取る（無ければ空）。UI スレッドだけが呼ぶ。
class ProjectSearchPort
{
  public:
    ProjectSearchPort() = default;
    virtual ~ProjectSearchPort() = default;
    ProjectSearchPort(const ProjectSearchPort &) = delete;
    ProjectSearchPort(ProjectSearchPort &&) = delete;
    ProjectSearchPort &operator=(const ProjectSearchPort &) = delete;
    ProjectSearchPort &operator=(ProjectSearchPort &&) = delete;

    virtual void request(ProjectSearchRequest request) = 0;
    virtual void cancel(std::uint64_t ticket) = 0;
    [[nodiscard]] virtual std::vector<ProjectSearchBatch> collect() = 0;
};
} // namespace nenenib::application
