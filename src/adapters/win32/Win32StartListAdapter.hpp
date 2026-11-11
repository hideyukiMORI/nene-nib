#pragma once

#include "BookmarkPort.hpp"
#include "HistoryPort.hpp"
#include "StartListPort.hpp"
#include "StartListShelf.hpp"
#include "Win32Worker.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace nenenib::adapters::win32
{
// 起動画面の履歴とブックマークを裏で 1 回読む（ADR 0172 の決定 3）。読みはワーカーを借りて、Ctrl+P
// の面と同じ HistoryPort::read と BookmarkPort::read を呼ぶ（読み口を 2 本にしない）。結果は棚に
// たまり、「届いた」の合図は bind で後から受け取る。HWND も窓メッセージの番号も知らない。
// history と bookmarks はワーカーより先に作って後に壊す（仕事が借りる）。
class Win32StartListAdapter final : public application::StartListPort
{
  public:
    Win32StartListAdapter(Win32Worker &worker, application::HistoryPort &history,
                          application::BookmarkPort &bookmarks);
    ~Win32StartListAdapter() override;
    Win32StartListAdapter(const Win32StartListAdapter &) = delete;
    Win32StartListAdapter(Win32StartListAdapter &&) = delete;
    Win32StartListAdapter &operator=(const Win32StartListAdapter &) = delete;
    Win32StartListAdapter &operator=(Win32StartListAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から値ありになったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void request() override;
    [[nodiscard]] std::optional<application::StartLists> collect() override;

  private:
    Win32Worker &worker_;
    application::HistoryPort &history_;
    application::BookmarkPort &bookmarks_;
    std::shared_ptr<StartListShelf> shelf_;
};
} // namespace nenenib::adapters::win32
