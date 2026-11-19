#pragma once

#include "RevealFailure.hpp"

#include <expected>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// エクスプローラーで場所を開く仕事と UI スレッドが共有する棚（ADR 0078 の追補 #1790・
// StartListShelf と同じ形）。たまりは終わった頼みの結果を頼んだ順に持つ。たまりと「届いた」の合図を
// 1 つのロックで守る。仕事が shared_ptr で持つので、adapter
// が先に壊れても残る。合図はたまりが空から 1 つ以上になったときだけ、ロックの外で呼ぶ。
class RevealShelf final
{
  public:
    using Result = std::expected<void, application::RevealFailure>;

    RevealShelf() = default;

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
