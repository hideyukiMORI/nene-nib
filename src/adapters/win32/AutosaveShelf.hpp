#pragma once

#include "AutosaveOutcome.hpp"
#include "EventHandle.hpp"

#include <windows.h>

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// 自動退避の仕事と UI スレッドが共有する棚（ADR 0137 の決定 4・5）。結果のたまり・「終わった」の
// 合図・受け付けの印・走っている仕事の数を 1 つのロックで守る。仕事が shared_ptr
// で持つので、adapter が先に壊れても残る。仕事が 0 になると event を立て、clear はそれを待つ。
// 合図は仕事の結果を 1 件積むたびに 1 回、ロックの外で呼ぶ（成功でも失敗でも・独立レビュー F3）。
class AutosaveShelf final
{
  public:
    AutosaveShelf();

    void bind(std::function<void()> finished);
    // 閉じる前なら true（store と discard が仕事を載せる前に見る）。
    [[nodiscard]] bool open() const;
    // 仕事の始め。閉じる前なら走っている数を 1 増やして true。閉じた後に順番が来た仕事は false で、
    // 何もしない（clear は順番待ちの仕事を待たない）。
    [[nodiscard]] bool begin();
    // begin が true だった仕事の終わり。走っている数が 0 になれば event を立てる。
    void end();
    // 仕事 1 つの結果を積み、合図を 1 回呼ぶ（閉じた後は積むだけで呼ばない）。
    void finish(application::AutosaveOutcome outcome);
    [[nodiscard]] std::vector<application::AutosaveOutcome> take();
    // 以後の仕事を受けず、合図も呼ばない。走っている仕事の終わりを limit ミリ秒まで待ち、
    // 終わったなら true。時計は読まない（OS の待ちの上限だけ）。
    [[nodiscard]] bool close(DWORD limit);

  private:
    mutable std::mutex mutex_;
    std::vector<application::AutosaveOutcome> outcomes_;
    std::function<void()> finished_;
    std::size_t running_ = 0;
    bool closed_ = false;
    EventHandle idle_;
};
} // namespace nenenib::adapters::win32
