#pragma once

#include "KernelHandle.hpp"
#include "PipeRead.hpp"
#include "PipeWrite.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"

#include <windows.h>

#include <cstddef>
#include <expected>
#include <memory>
#include <string>

namespace nenenib::adapters::win32
{
// stop で子の終了を待つ時間（ミリ秒）。過ぎたら Job Object ごと殺す（ADR 0160 の決定 2）。
inline constexpr DWORD server_stop_wait_milliseconds = 2000;

// 起きている言語サーバー 1 つ: 子プロセス・Job Object・標準入出力の名前つきパイプ 2 本と、
// その上の読み 1 本と書きの行列（ADR 0160 の決定 2）。置き場所を動かさない（heap に 1 つ）。
// 標準入出力は FILE_FLAG_OVERLAPPED の名前つきパイプ、標準エラーは NUL へ捨てる。子は
// CREATE_SUSPENDED で作り、KILL_ON_JOB_CLOSE の Job Object に入れてから走らせる（Nib
// が落ちても残らない）。
class ServerChild final
{
  public:
    explicit ServerChild(std::size_t read_bytes);
    ~ServerChild();
    ServerChild(const ServerChild &) = delete;
    ServerChild(ServerChild &&) = delete;
    ServerChild &operator=(const ServerChild &) = delete;
    ServerChild &operator=(ServerChild &&) = delete;

    // 子を起こす。1 つの値につき 1 回だけ呼ぶ。失敗したら作りかけの資源は end が片付ける。
    [[nodiscard]] std::expected<void, application::ServerFailure>
    spawn(const application::ServerLaunch &launch);
    [[nodiscard]] HANDLE process() const noexcept;
    [[nodiscard]] PipeRead &reader() noexcept;
    [[nodiscard]] PipeWrite &writer() noexcept;
    // 書きを取消して入力のパイプを閉じ、読みを取消して出力のパイプを閉じ（どれも最終完了を待つ・
    // ADR 0126）、子の終了を server_stop_wait_milliseconds だけ待つ。終わらなければ Job Object
    // で殺して待つ。
    void end() noexcept;

  private:
    [[nodiscard]] bool connect(KernelHandle &child_input, KernelHandle &child_output);
    [[nodiscard]] std::expected<void, application::ServerFailure>
    create(const std::wstring &executable, const application::ServerLaunch &launch,
           const KernelHandle &child_input, const KernelHandle &child_output);

    std::size_t read_bytes_;
    KernelHandle input_;
    KernelHandle output_;
    KernelHandle job_;
    KernelHandle process_;
    std::unique_ptr<PipeWrite> writer_;
    std::unique_ptr<PipeRead> reader_;
};
} // namespace nenenib::adapters::win32
