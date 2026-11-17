#pragma once

#include "CodePagePort.hpp"
#include "PreviewBatch.hpp"
#include "PreviewPort.hpp"
#include "PreviewRequest.hpp"
#include "PreviewShelf.hpp"
#include "Win32Worker.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace nenenib::adapters::win32
{
// 探して見る面のプレビューの窓の本物（ADR 0189 の決定 4・Win32ProjectSearchAdapter と同じ形）。
// 読みはワーカーを借りて 1 仕事ずつ走り（PreviewRead）、届いた窓は棚にたまる。「届いた」の合図は
// bind で後から受け取る（渡されるまでは知らせない）。HWND も窓メッセージの番号も知らない。
class Win32PreviewAdapter final : public application::PreviewPort
{
  public:
    Win32PreviewAdapter(Win32Worker &worker, application::CodePagePort &code_pages);
    ~Win32PreviewAdapter() override;
    Win32PreviewAdapter(const Win32PreviewAdapter &) = delete;
    Win32PreviewAdapter(Win32PreviewAdapter &&) = delete;
    Win32PreviewAdapter &operator=(const Win32PreviewAdapter &) = delete;
    Win32PreviewAdapter &operator=(Win32PreviewAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void request(application::PreviewRequest request) override;
    void cancel(std::uint64_t ticket) override;
    [[nodiscard]] std::vector<application::PreviewBatch> collect() override;

  private:
    Win32Worker &worker_;
    application::CodePagePort &code_pages_;
    std::shared_ptr<PreviewShelf> shelf_;
};
} // namespace nenenib::adapters::win32
