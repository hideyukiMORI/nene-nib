#pragma once

#include "LinkOpenFailure.hpp"

#include <expected>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// リンクを OS に渡す仕事と UI スレッドが共有する棚（#1869 の段 L2・#1790 の RevealShelf
// と同じ形）。 たまりは終わった頼みの結果を頼んだ順に持つ。たまりと「届いた」の合図を 1
// つのロックで守る。仕事が shared_ptr で持つので、adapter が先に壊れても残る。合図はたまりが空から
// 1 つ以上になったとき だけ、ロックの外で呼ぶ。
class LinkOpenShelf final
{
  public:
    using Result = std::expected<void, application::LinkOpenFailure>;

    LinkOpenShelf() = default;

    void bind(std::function<void()> arrived);
    // 以後の結果を受けず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    void deliver(Result result);
    [[nodiscard]] std::vector<Result> take();

  private:
    std::mutex mutex_;
    bool closed_ = false;
    std::vector<Result> results_;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
