#pragma once

#include "StartLists.hpp"

#include <functional>
#include <mutex>
#include <optional>

namespace nenenib::adapters::win32
{
// 起動画面の読みの仕事と UI スレッドが共有する棚（ADR 0172 の決定 3・MarkdownShelf と同じ形）。
// たまりは読んだ結果 1 つだけ。たまりと「届いた」の合図を 1 つのロックで守る。仕事が shared_ptr で
// 持つので、adapter が先に壊れても残る。合図はたまりが空から値ありになったときだけ、ロックの外で
// 呼ぶ。
class StartListShelf final
{
  public:
    StartListShelf() = default;

    void bind(std::function<void()> arrived);
    // 以後の結果を受けず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    // 結果を置く（たまりにあれば置き換える）。閉じた後は置かない。
    void deliver(application::StartLists lists);
    [[nodiscard]] std::optional<application::StartLists> take();

  private:
    std::mutex mutex_;
    bool closed_ = false;
    std::optional<application::StartLists> lists_;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
