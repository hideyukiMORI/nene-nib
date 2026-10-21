#pragma once

#include "MarkdownOutcome.hpp"
#include "MarkdownPort.hpp"
#include "MarkdownRequest.hpp"
#include "MarkdownShelf.hpp"
#include "Win32Worker.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace nenenib::adapters::win32
{
// Markdown の本文を裏で解析する（ADR 0155 の決定 5・6）。解析はワーカーを借りて走り、
// 結果は棚にたまる。新しい要求が来るか adapter が壊れると、走っている解析は md4c の次の
// コールバックで打ち切り、stopped の結果を棚に置く（application が券で捨てる）。
// 「届いた」の合図は bind で後から受け取る。HWND も窓メッセージの番号も知らない。
class Win32MarkdownAdapter final : public application::MarkdownPort
{
  public:
    explicit Win32MarkdownAdapter(Win32Worker &worker);
    ~Win32MarkdownAdapter() override;
    Win32MarkdownAdapter(const Win32MarkdownAdapter &) = delete;
    Win32MarkdownAdapter(Win32MarkdownAdapter &&) = delete;
    Win32MarkdownAdapter &operator=(const Win32MarkdownAdapter &) = delete;
    Win32MarkdownAdapter &operator=(Win32MarkdownAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void parse(application::MarkdownRequest request) override;
    [[nodiscard]] std::vector<application::MarkdownOutcome> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<MarkdownShelf> shelf_;
};
} // namespace nenenib::adapters::win32
