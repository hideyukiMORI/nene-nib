#pragma once

#include "EventHandle.hpp"

#include <windows.h>

#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーの標準出力のパイプに常に 1 本だけ出す OVERLAPPED の ReadFile（ADR 0160 の決定 2）。
// OVERLAPPED・専用の manual-reset event・buffer はこの値が持ち、置き場所を動かさない（ADR 0126
// の決定 2）。パイプの handle は借りるだけで閉じない。読みが出ている間に壊すときは drain
// で最終完了を待つ。失敗は Win32 のエラーの値（ERROR_BROKEN_PIPE は相手の終わり・
// ERROR_IO_INCOMPLETE はまだ）。
class PipeRead final
{
  public:
    PipeRead(HANDLE pipe, std::size_t bytes);
    ~PipeRead();
    PipeRead(const PipeRead &) = delete;
    PipeRead(PipeRead &&) = delete;
    PipeRead &operator=(const PipeRead &) = delete;
    PipeRead &operator=(PipeRead &&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] HANDLE event() const noexcept;
    // ReadFile を 1 本出す。即時の完了も event が立つので、読めた量は finish で受け取る。
    [[nodiscard]] std::expected<void, DWORD> issue();
    // event が立った後に呼ぶ。読めたバイト列（次の issue まで有効）か、終わりのエラー。
    [[nodiscard]] std::expected<std::string_view, DWORD> finish();
    // 取消を頼み、最終完了を待ってから戻る（取消の成功だけで資源を離さない・ADR 0126 の決定 6・
    // 9）。
    void drain() noexcept;

  private:
    HANDLE pipe_;
    EventHandle event_;
    OVERLAPPED overlapped_{};
    std::vector<char> buffer_;
    bool pending_ = false;
};
} // namespace nenenib::adapters::win32
