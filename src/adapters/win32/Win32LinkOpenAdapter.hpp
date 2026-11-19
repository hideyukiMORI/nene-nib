#pragma once

#include "LinkOpenFailure.hpp"
#include "LinkOpenPort.hpp"
#include "LinkOpenShelf.hpp"
#include "Win32Worker.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// リンクを OS の既定の道（ブラウザ・メール）に渡す本物（#1869 の段 L2・#1790 の
// Win32RevealAdapter と同じ形）。シェルの呼び出しはワーカーを借りて 1 仕事ずつ走り（UI スレッドを
// 止めない）、結果は棚にたまる。「届いた」の合図は bind で後から受け取る（渡されるまでは知らせ
// ない）。HWND も窓メッセージの番号も知らない。
class Win32LinkOpenAdapter final : public application::LinkOpenPort
{
  public:
    explicit Win32LinkOpenAdapter(Win32Worker &worker);
    ~Win32LinkOpenAdapter() override;
    Win32LinkOpenAdapter(const Win32LinkOpenAdapter &) = delete;
    Win32LinkOpenAdapter(Win32LinkOpenAdapter &&) = delete;
    Win32LinkOpenAdapter &operator=(const Win32LinkOpenAdapter &) = delete;
    Win32LinkOpenAdapter &operator=(Win32LinkOpenAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void open(std::string uri) override;
    [[nodiscard]] std::vector<std::expected<void, application::LinkOpenFailure>> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<LinkOpenShelf> shelf_;
};
} // namespace nenenib::adapters::win32
