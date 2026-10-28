#pragma once

#include "LanguageServerPort.hpp"
#include "ServerChild.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerFramer.hpp"
#include "ServerLaunch.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 1 回の ReadFile で受ける量の既定値。
inline constexpr std::size_t language_server_read_bytes = std::size_t{64} * 1024;

// 言語サーバーの子プロセスと OVERLAPPED のパイプ（ADR 0160 の決定 2・6）。スレッドを作らず、
// Win32Worker も使わない。I/O の完了と子の終了は handle の立ちで知り、その待ちは UI スレッドの
// pump が waitable_handles を MsgWaitForMultipleObjectsEx の配列に足して行う（HANDLE は port
// に出さない）。立った handle を completed に渡すと、I/O を回収して出来事の列に積み、列が空から 1
// つ以上になったときだけ構築時に受けた合図を呼ぶ（Win32FolderAdapter の合図と同じ形・本番は
// work_message を送る）。全部の口は UI スレッドだけが呼ぶ。取消と完了の分離は ADR
// 0126（資源は最終完了の後だけ離す）。
class Win32LanguageServer final : public application::LanguageServerPort
{
  public:
    explicit Win32LanguageServer(std::function<void()> arrived,
                                 std::size_t read_bytes = language_server_read_bytes);
    ~Win32LanguageServer() override;
    Win32LanguageServer(const Win32LanguageServer &) = delete;
    Win32LanguageServer(Win32LanguageServer &&) = delete;
    Win32LanguageServer &operator=(const Win32LanguageServer &) = delete;
    Win32LanguageServer &operator=(Win32LanguageServer &&) = delete;

    [[nodiscard]] std::expected<void, application::ServerFailure>
    start(application::ServerLaunch launch) override;
    void send(std::string body) override;
    void stop() override;
    [[nodiscard]] std::vector<application::ServerEvent> collect() override;

    // pump が待つ handle（出ている読み・出ている書き・まだ終わっていない子）。次に start / send /
    // stop / completed を呼ぶまで有効。
    [[nodiscard]] std::span<const HANDLE> waitable_handles() const noexcept;
    // waitable_handles の 1 つが立ったときに呼ぶ。知らない handle は何もしない。
    void completed(HANDLE handle);

  private:
    void post(application::ServerEvent event);
    void issue_read();
    void read_completed();
    void read_ended(DWORD error);
    void write_completed();
    void write_failed();
    void process_completed();
    void finish_if_exited();
    void refresh();

    std::function<void()> arrived_;
    std::size_t read_bytes_;
    std::unique_ptr<ServerChild> child_;
    ServerFramer framer_;
    std::vector<application::ServerEvent> events_;
    std::vector<HANDLE> waitable_;
    std::optional<std::int32_t> exit_code_;
    bool output_open_ = false;
    bool writable_ = false;
};
} // namespace nenenib::adapters::win32
