#pragma once

#include "LanguageServerEvent.hpp"
#include "LanguageServerPort.hpp"
#include "LanguageServerSlot.hpp"
#include "ServerChild.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
// 1 回の ReadFile で受ける量の既定値。
inline constexpr std::size_t language_server_read_bytes = std::size_t{64} * 1024;
// 退く子を同時に残す上限（2026-11-20 の追補・段 S2）。超えたら最も古い退く子を Job Object ごと
// 殺して片付ける（待たない）。待ちの配列は席ごとに 4 本（読み・標準エラーの読み・書き・子・
// 2026-11-22 の追補）とこの数（退く子は標準エラーを読まないので子の 1 本だけ）で、
// MAXIMUM_WAIT_OBJECTS の内に収まる（.cpp の static_assert）。
inline constexpr std::size_t language_server_retired_limit = 8;
// 合図を待つ間に席ごとに積んでおく標準エラーの上限（2026-11-22 の追補）。controller の記録の
// 上限（256 KiB）と同じで、超えたら古いバイトから捨てる。
inline constexpr std::size_t language_server_error_queue_bytes = std::size_t{256} * 1024;

// 出来事の列 events に席 slot の標準エラーのバイト列を積む（2026-11-22 の追補・#1950 のレビュー
// R1）。その席の最後の出来事が標準エラーの塊なら繋ぎ（あいだにほかの席の出来事があっても・
// 同じ席の本文との前後は保つ）、そうでなければ新しい塊を積む。列の中のその席の塊のバイトの和が
// language_server_error_queue_bytes を超えたら、古い塊の古いバイトから捨てる（空になった塊は
// 列から外す）。合図は出さない。空のバイト列は何もしない。OS に触れない。
void queue_error_output(std::vector<application::LanguageServerEvent> &events,
                        application::LanguageServerSlot slot, std::string_view bytes);

// 言語サーバーの子プロセスと OVERLAPPED のパイプ（ADR 0160 の決定 2・6）。
// スレッドを作らず、Win32Worker も使わない。席（LanguageServerSlot）ごとに
// 子・パイプ・Job Object を 1 組持つ（2026-11-20 の追補）。I/O の完了と子の終了は
// handle の立ちで知り、その待ちは UI スレッドの pump が waitable_handles を
// MsgWaitForMultipleObjectsEx の配列に足して行う（HANDLE は port に出さない）。
// 立った handle を completed に渡すと、I/O を回収してその席の出来事として列に積み、
// 前の合図がまだ collect されていなければ呼ばず、そうでなければ構築時に受けた
// 合図を呼ぶ（Win32FolderAdapter の合図と同じ形・本番は work_message を送る）。
// 標準エラーの塊（ServerErrorOutput）は合図を出さずに列に積み（打鍵ごとに書く
// サーバーで合図を増やさない・2026-11-22 の追補）、queue_error_output の 1 本で席ごとに
// language_server_error_queue_bytes までの新しい分を持つ。stop は子を待たない
// （退く子の process も待ちの配列に載る）。stop は閉じる前に完了済みの読みを回収し、その席の
// 出来事を列から外して返す（#1950 のレビュー M1）。全部の口は UI スレッドだけが呼ぶ。
// 取消と完了の分離は ADR 0126（資源は最終完了の後だけ離す）。
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
    start(application::LanguageServerSlot slot, application::ServerLaunch launch) override;
    void send(application::LanguageServerSlot slot, std::string body) override;
    [[nodiscard]] std::vector<application::ServerEvent>
    stop(application::LanguageServerSlot slot) override;
    [[nodiscard]] std::vector<application::LanguageServerEvent> collect() override;

    // pump が待つ handle（席ごとの出ている読み・出ている書き・起きている子と、退く子）。次に
    // start / send / stop / completed / expire_retired を呼ぶまで有効。
    [[nodiscard]] std::span<const HANDLE> waitable_handles() const noexcept;
    // 届いたの合図を結び直す（合成ルートが窓を作った後に 1 回・Win32FolderAdapter の bind と
    // 同じ形）。空の合図は呼ばない。結ぶ前に溜まった出来事があれば、結んだ合図を 1 回呼ぶ。
    void bind(std::function<void()> arrived);
    // waitable_handles の 1 つが立ったときに呼ぶ。知らない handle は何もしない。
    void completed(HANDLE handle);
    // stop の後にまだ終わっていない退く子を Job Object ごと殺して片付ける（ADR 0160 の追補・
    // controller が LanguageServerTick で呼ぶ）。待たない。
    void expire_retired() override;

  private:
    [[nodiscard]] bool retire_finished(HANDLE handle);
    void children_completed(HANDLE handle);
    void child_completed(std::size_t index, HANDLE handle);
    void post(std::size_t index, application::ServerEvent event);
    void issue_read(std::size_t index);
    void read_completed(std::size_t index);
    void issue_error_read(std::size_t index);
    void error_read_completed(std::size_t index);
    void drain_errors(std::size_t index);
    void post_error_output(std::size_t index, std::string_view bytes);
    [[nodiscard]] bool deliver(std::size_t index, std::string_view bytes);
    void read_ended(std::size_t index, DWORD error);
    void write_completed(std::size_t index);
    void write_failed(std::size_t index);
    void process_completed(std::size_t index);
    void drain_output(std::size_t index);
    void retire(std::unique_ptr<ServerChild> child);
    void refresh();

    std::function<void()> arrived_;
    std::size_t read_bytes_;
    // 席ごとの起きている子（無ければ空）。
    std::array<std::unique_ptr<ServerChild>, application::language_server_slot_limit> children_;
    // stop でパイプを閉じ、終わりを待っている子（process の立ちか expire_retired
    // で片付く・古い順）。
    std::vector<std::unique_ptr<ServerChild>> retired_;
    // 席ごとの、受けたがまだ枠に揃っていないバイト列（core の lsp_frames の consumed より後ろ・
    // ADR 0160 の決定 3）。start で空にする。
    std::array<std::string, application::language_server_slot_limit> pending_;
    std::vector<application::LanguageServerEvent> events_;
    std::vector<HANDLE> waitable_;
    std::array<bool, application::language_server_slot_limit> writable_{};
    // 合図を出してまだ collect されていない（2026-11-22 の追補）。合図を出さない標準エラーの塊が
    // 列に残っていても、次の出来事で 1 回だけ合図を出すための印。collect で戻す。
    bool signalled_ = false;
};
} // namespace nenenib::adapters::win32
