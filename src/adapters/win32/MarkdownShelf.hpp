#pragma once

#include "MarkdownOutcome.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// Markdown の解析の仕事と UI スレッドが共有する棚（ADR 0155 の決定 5・6・FolderShelf と同じ形）。
// たまりは最新の結果 1 つだけで、新しい結果が来たら古いものを置き換える（上限 1）。
// たまりと「届いた」の合図を 1 つのロックで守り、今の券と閉じた印は不可分な値で持つ
// （解析の打ち切りの問いは md4c のコールバックのたびに来るので、ロックを取らずに読む）。
// 仕事が shared_ptr で持つので、adapter が先に壊れても残る。打ち切った結果（stopped）も積む
// （捨てるのは application・決定 6）。合図はたまりが空から 1
// つ以上になったときだけ、ロックの外で呼ぶ。
class MarkdownShelf final
{
  public:
    MarkdownShelf() = default;

    void bind(std::function<void()> arrived);
    // 新しい要求の券を今の券にする。前の券の解析は次の問いで打ち切る。
    void renew(std::uint64_t ticket) noexcept;
    // 以後の結果を受けず、合図も呼ばず、走っている解析を打ち切る（adapter が壊れるとき）。
    void close();
    // 券の解析を続けてよいか（閉じておらず、券が今の券）。
    [[nodiscard]] bool wanted(std::uint64_t ticket) const noexcept;
    // 結果を置く（たまりにあれば置き換える）。閉じた後は置かない。
    void deliver(application::MarkdownOutcome outcome);
    [[nodiscard]] std::vector<application::MarkdownOutcome> take();

  private:
    std::atomic<std::uint64_t> latest_ticket_ = 0;
    std::atomic<bool> closed_ = false;
    std::mutex mutex_;
    std::vector<application::MarkdownOutcome> outcomes_;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
