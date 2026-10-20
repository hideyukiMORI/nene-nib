#pragma once

#include "MarkdownOutcome.hpp"
#include "MarkdownPort.hpp"
#include "MarkdownRequest.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace nenenib::adapters::win32
{
// Markdown の本文を裏で解析する（ADR 0155 の決定 5）。いまは骨だけで、parse は券を覚えるだけで
// 解析を頼まず、collect は空、bind は合図を覚えるだけ（呼ばない）。ワーカーで parse_markdown を
// 回し、棚が空から 1 つ以上になったときに合図する本物は #801 が入れる。
class Win32MarkdownAdapter final : public application::MarkdownPort
{
  public:
    Win32MarkdownAdapter() = default;
    ~Win32MarkdownAdapter() override = default;
    Win32MarkdownAdapter(const Win32MarkdownAdapter &) = delete;
    Win32MarkdownAdapter(Win32MarkdownAdapter &&) = delete;
    Win32MarkdownAdapter &operator=(const Win32MarkdownAdapter &) = delete;
    Win32MarkdownAdapter &operator=(Win32MarkdownAdapter &&) = delete;

    void bind(std::function<void()> arrived);
    void parse(application::MarkdownRequest request) override;
    [[nodiscard]] std::vector<application::MarkdownOutcome> collect() override;

  private:
    std::function<void()> arrived_;
    std::uint64_t last_ticket_ = 0;
};
} // namespace nenenib::adapters::win32
