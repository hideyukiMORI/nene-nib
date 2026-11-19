#pragma once

#include "FilePath.hpp"
#include "RevealFailure.hpp"
#include "RevealPort.hpp"
#include "RevealShelf.hpp"
#include "Win32Worker.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <vector>

namespace nenenib::adapters::win32
{
// エクスプローラーで場所を開く本物（ADR 0078 の追補 #1790・Win32StartListAdapter と同じ形）。
// シェルの呼び出しはワーカーを借りて 1 仕事ずつ走り（UI スレッドを止めない）、結果は棚にたまる。
// 「届いた」の合図は bind で後から受け取る（渡されるまでは知らせない）。HWND も窓メッセージの番号も
// 知らない。
class Win32RevealAdapter final : public application::RevealPort
{
  public:
    explicit Win32RevealAdapter(Win32Worker &worker);
    ~Win32RevealAdapter() override;
    Win32RevealAdapter(const Win32RevealAdapter &) = delete;
    Win32RevealAdapter(Win32RevealAdapter &&) = delete;
    Win32RevealAdapter &operator=(const Win32RevealAdapter &) = delete;
    Win32RevealAdapter &operator=(Win32RevealAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void reveal(core::FilePath path) override;
    [[nodiscard]] std::vector<std::expected<void, application::RevealFailure>> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<RevealShelf> shelf_;
};
} // namespace nenenib::adapters::win32
