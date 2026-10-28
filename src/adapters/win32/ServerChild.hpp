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

    // 子を起こす。1 つの値につき 1 回だけ呼ぶ。失敗したら作りかけの資源はデストラクタが片付ける。
    [[nodiscard]] std::expected<void, application::ServerFailure>
    spawn(const application::ServerLaunch &launch);
    [[nodiscard]] HANDLE process() const noexcept;
    [[nodiscard]] PipeRead &reader() noexcept;
    [[nodiscard]] PipeWrite &writer() noexcept;
    // 書きを取消して入力のパイプを閉じ、読みを取消して出力のパイプを閉じる（どれも最終完了を待つ
    // ・ADR 0126）。子は待たない（子は自分の入力の EOF で終わる）。以後 reader と writer
    // は呼ばない。
    void close_pipes() noexcept;
    // Job Object の中の子と子孫を全部殺す（ADR 0160 の追補）。待たない（終わりは process
    // の立ちで知る）。
    void terminate() noexcept;

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
