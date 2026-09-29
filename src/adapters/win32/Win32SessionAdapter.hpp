#pragma once

#include "FilePort.hpp"
#include "SessionPort.hpp"

#include <expected>
#include <optional>

namespace nenenib::adapters::win32
{
// 前回のタブの一覧の置き場（ADR 0059 の決定 2）。設定と同じ親の session.v1。
[[nodiscard]] std::expected<core::FilePath, application::SessionFailure> local_session_path();

// session.v1 を FilePort で読み書きする。設定と違い、lock も「外で変わっていたら上書きしない」も
// 持たない（窓を 2 つ開いていたら後から閉じたほうが残る・ADR 0059 の決定 2）。
class Win32SessionAdapter final : public application::SessionPort
{
  public:
    Win32SessionAdapter(application::FilePort &files,
                        std::expected<core::FilePath, application::SessionFailure> path);
    [[nodiscard]] std::expected<std::optional<application::Session>, application::SessionFailure>
    read() override;
    [[nodiscard]] std::expected<void, application::SessionFailure>
    write(const application::Session &session) override;

  private:
    application::FilePort &files_;
    std::expected<core::FilePath, application::SessionFailure> path_;
};
} // namespace nenenib::adapters::win32
