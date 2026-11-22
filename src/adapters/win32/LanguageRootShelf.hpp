#pragma once

#include "LanguageRootAnswer.hpp"

#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーの root を探す仕事と UI スレッドが共有する棚（ADR 0161 の 2026-11-22 の追補・段 S3・
// RevealShelf と同じ形）。たまりは終わった頼みの答えを終わった順に持つ。たまりと「届いた」の合図を
// 1 つのロックで守る。仕事が shared_ptr で持つので、adapter が先に壊れても残る。合図はたまりが空
// から 1 つ以上になったときだけ、ロックの外で呼ぶ。
class LanguageRootShelf final
{
  public:
    LanguageRootShelf() = default;

    void bind(std::function<void()> arrived);
    // 以後の答えを受けず、合図も呼ばない（adapter が壊れるとき）。
    void close();
    void deliver(application::LanguageRootAnswer answer);
    [[nodiscard]] std::vector<application::LanguageRootAnswer> take();

  private:
    std::mutex mutex_;
    bool closed_ = false;
    std::vector<application::LanguageRootAnswer> answers_;
    std::function<void()> arrived_;
};
} // namespace nenenib::adapters::win32
