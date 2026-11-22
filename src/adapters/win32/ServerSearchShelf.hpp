#pragma once

#include "FoundLanguageServer.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーの探しの仕事と UI スレッドが共有する棚（LSP 段 S4・#1945・StartListShelf と同じ形）。
// たまりは最後の探しの結果 1 つだけ。たまりと「届いた」の合図を 1 つのロックで守る。仕事が
// shared_ptr で持つので、adapter が先に壊れても残る。合図はたまりが空から値ありになったときだけ、
// ロックの外で呼ぶ。
class ServerSearchShelf final
{
  public:
    using Found = std::vector<std::optional<application::FoundLanguageServer>>;

    ServerSearchShelf() = default;

    void bind(std::function<void()> arrived);
    // 以後の結果を受けず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    // 結果を置く（たまりにあれば置き換える）。閉じた後は置かない。
    void deliver(Found found);
    [[nodiscard]] std::optional<Found> take();

  private:
    std::mutex mutex_;
    bool closed_ = false;
    std::optional<Found> found_;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
