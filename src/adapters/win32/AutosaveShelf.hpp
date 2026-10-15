#pragma once

#include "AutosaveFailure.hpp"
#include "EventHandle.hpp"

#include <windows.h>

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// 自動退避の仕事と UI スレッドが共有する棚（ADR 0137 の決定 4・5）。失敗のたまり・「失敗した」の
// 合図・受け付けの印・走っている仕事の数を 1 つのロックで守る。仕事が shared_ptr
// で持つので、adapter が先に壊れても残る。仕事が 0 になると event を立て、clear はそれを待つ。
// 合図はたまりが空から 1 つ以上になったときだけ、ロックの外で呼ぶ（FolderShelf と同じ形）。
class AutosaveShelf final
{
  public:
    AutosaveShelf();

    void bind(std::function<void()> failed);
    // 閉じる前なら true（store と discard が仕事を載せる前に見る）。
    [[nodiscard]] bool open() const;
    // 仕事の始め。閉じる前なら走っている数を 1 増やして true。閉じた後に順番が来た仕事は false で、
    // 何もしない（clear は順番待ちの仕事を待たない）。
    [[nodiscard]] bool begin();
    // begin が true だった仕事の終わり。走っている数が 0 になれば event を立てる。
    void end();
    void fail(application::AutosaveFailure failure);
    [[nodiscard]] std::vector<application::AutosaveFailure> take();
    // 以後の仕事を受けず、合図も呼ばない。走っている仕事の終わりを limit ミリ秒まで待ち、
    // 終わったなら true。時計は読まない（OS の待ちの上限だけ）。
    [[nodiscard]] bool close(DWORD limit);

  private:
    mutable std::mutex mutex_;
    std::vector<application::AutosaveFailure> failures_;
    std::function<void()> failed_;
    std::size_t running_ = 0;
    bool closed_ = false;
    EventHandle idle_;
};
} // namespace nenenib::adapters::win32
