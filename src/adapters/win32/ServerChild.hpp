#pragma once

#include "KernelHandle.hpp"
#include "PipeRead.hpp"
#include "PipeWrite.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>

namespace nenenib::adapters::win32
{
// 起きている言語サーバー 1 つ: 子プロセス・Job Object・標準入出力と標準エラーの名前つきパイプ
// 3 本と、その上の読み 2 本（標準出力と標準エラー）と書きの行列（ADR 0160 の決定 2・2026-11-22 の
// 追補）。置き場所を動かさない（heap に 1 つ）。どのパイプも FILE_FLAG_OVERLAPPED の名前つき
// パイプ。子は CREATE_SUSPENDED で作り、KILL_ON_JOB_CLOSE の Job Object に入れてから走らせる
// （Nib が落ちても残らない）。
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
    // 標準エラーの読み（ADR 0160 の 2026-11-22 の追補）。
    [[nodiscard]] PipeRead &error_reader() noexcept;
    [[nodiscard]] PipeWrite &writer() noexcept;
    // 書きを取消して入力のパイプを閉じ、読みを取消して出力と標準エラーのパイプを閉じる（どれも
    // 最終完了を待つ・ADR 0126）。子は待たない（子は自分の入力の EOF で終わる）。以後 reader・
    // error_reader・writer は呼ばない（退く子の標準エラーは読まない）。
    void close_pipes() noexcept;
    // Job Object の中の子と子孫を全部殺す（ADR 0160 の追補）。待たない（終わりは process
    // の立ちで知る）。
    void terminate() noexcept;

  private:
    // 子の端（標準入力・標準出力・標準エラーの順）。
    using ChildEnds = std::array<KernelHandle, 3>;
    [[nodiscard]] bool connect(ChildEnds &ends);
    [[nodiscard]] std::expected<void, application::ServerFailure>
    create(const std::wstring &executable, const application::ServerLaunch &launch,
           const ChildEnds &ends);

    std::size_t read_bytes_;
    KernelHandle input_;
    KernelHandle output_;
    KernelHandle error_;
    KernelHandle job_;
    KernelHandle process_;
    std::unique_ptr<PipeWrite> writer_;
    std::unique_ptr<PipeRead> reader_;
    std::unique_ptr<PipeRead> error_reader_;
};
} // namespace nenenib::adapters::win32
