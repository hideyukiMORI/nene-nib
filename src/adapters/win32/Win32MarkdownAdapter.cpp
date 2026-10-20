#include "Win32MarkdownAdapter.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
void Win32MarkdownAdapter::bind(std::function<void()> arrived)
{
    arrived_ = std::move(arrived);
}

// 骨（#801 までの仮置き）。券だけを覚え、本文は解析しない。
void Win32MarkdownAdapter::parse(application::MarkdownRequest request)
{
    last_ticket_ = request.ticket;
}

std::vector<application::MarkdownOutcome> Win32MarkdownAdapter::collect()
{
    return {};
}
} // namespace nenenib::adapters::win32
