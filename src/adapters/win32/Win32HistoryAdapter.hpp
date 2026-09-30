#pragma once

#include "FilePort.hpp"
#include "HistoryPort.hpp"

#include <expected>

namespace nenenib::adapters::win32
{
// 閉じたファイルの履歴の置き場（ADR 0060 の決定 8）。設定と同じ親の history.v1。
[[nodiscard]] std::expected<core::FilePath, application::FileHistoryFailure> local_history_path();

// history.v1 を FilePort で読み書きする。lock も「外で変わっていたら上書きしない」も持たない。
// ほかの窓が書いた分は、書く側が読んでから記録して書くことで残す（ADR 0060 の決定 8）。
class Win32HistoryAdapter final : public application::HistoryPort
{
  public:
    Win32HistoryAdapter(application::FilePort &files,
                        std::expected<core::FilePath, application::FileHistoryFailure> path);
    [[nodiscard]] std::expected<application::FileHistory, application::FileHistoryFailure>
    read() override;
    [[nodiscard]] std::expected<void, application::FileHistoryFailure>
    write(const application::FileHistory &history) override;

  private:
    application::FilePort &files_;
    std::expected<core::FilePath, application::FileHistoryFailure> path_;
};
} // namespace nenenib::adapters::win32
