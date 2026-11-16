#pragma once

#include "ProjectSearchBatch.hpp"
#include "ProjectSearchPort.hpp"
#include "ProjectSearchRequest.hpp"
#include "ProjectSearchShelf.hpp"
#include "Win32Worker.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace nenenib::adapters::win32
{
// プロジェクトの全文検索の本物（ADR 0167 の決定 2〜4・Win32FolderAdapter と同じ形）。歩きと照合は
// ワーカーを借りて 1 仕事ずつ走り（ProjectSearchWalk）、届いた分は棚にたまる。「届いた」の合図は
// bind で後から受け取る（渡されるまでは知らせない）。HWND も窓メッセージの番号も知らない。
class Win32ProjectSearchAdapter final : public application::ProjectSearchPort
{
  public:
    explicit Win32ProjectSearchAdapter(Win32Worker &worker);
    ~Win32ProjectSearchAdapter() override;
    Win32ProjectSearchAdapter(const Win32ProjectSearchAdapter &) = delete;
    Win32ProjectSearchAdapter(Win32ProjectSearchAdapter &&) = delete;
    Win32ProjectSearchAdapter &operator=(const Win32ProjectSearchAdapter &) = delete;
    Win32ProjectSearchAdapter &operator=(Win32ProjectSearchAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void request(application::ProjectSearchRequest request) override;
    void cancel(std::uint64_t ticket) override;
    [[nodiscard]] std::vector<application::ProjectSearchBatch> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<ProjectSearchShelf> shelf_;
};
} // namespace nenenib::adapters::win32
