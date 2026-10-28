#pragma once

#include "EventHandle.hpp"

#include <windows.h>

#include <cstddef>
#include <deque>
#include <expected>
#include <string>

namespace nenenib::adapters::win32
{
// 言語サーバーの標準入力のパイプへの OVERLAPPED の WriteFile と、その待ち行列（ADR 0160 の決定
// 2）。書くのは 1 本ずつで、出ている間に足された枠は行列に並ぶ（64 KB を超える didOpen
// でも止まらない）。OVERLAPPED・event・書いている枠はこの値が持ち、置き場所を動かさない（ADR 0126
// の決定 2）。パイプの handle は借りるだけで閉じない。失敗は Win32 のエラーの値
// （ERROR_IO_INCOMPLETE はまだ）。
class PipeWrite final
{
  public:
    explicit PipeWrite(HANDLE pipe);
    ~PipeWrite();
    PipeWrite(const PipeWrite &) = delete;
    PipeWrite(PipeWrite &&) = delete;
    PipeWrite &operator=(const PipeWrite &) = delete;
    PipeWrite &operator=(PipeWrite &&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] HANDLE event() const noexcept;
    // 枠を 1 つ行列に足し、書いていなければ書き始める。
    [[nodiscard]] std::expected<void, DWORD> push(std::string frame);
    // event が立った後に呼ぶ。書き終えた分を進め、残りか次の枠を書き始める。
    [[nodiscard]] std::expected<void, DWORD> finish();
    // 取消を頼んで最終完了を待ち、行列を捨てる（ADR 0126 の決定 6・9）。
    void drain() noexcept;

  private:
    [[nodiscard]] std::expected<void, DWORD> next();
    [[nodiscard]] std::expected<void, DWORD> begin();

    HANDLE pipe_;
    EventHandle event_;
    OVERLAPPED overlapped_{};
    std::string current_;
    std::size_t written_ = 0;
    std::deque<std::string> queue_;
    bool pending_ = false;
};
} // namespace nenenib::adapters::win32
