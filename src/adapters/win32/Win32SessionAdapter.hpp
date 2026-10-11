#pragma once

#include "FilePort.hpp"
#include "SessionPaths.hpp"
#include "SessionPort.hpp"

#include <expected>
#include <optional>

namespace nenenib::adapters::win32
{
// 前回のタブの一覧の置き場（ADR 0059 / 0114）。設定と同じ親の session.v2 と v1。
[[nodiscard]] std::expected<SessionPaths, application::SessionFailure> local_session_paths();

// session.v2 を書き、無いときだけv1を読む。設定と違い、lock も「外で変わっていたら上書きしない」も
// 持たない（窓を 2 つ開いていたら後から閉じたほうが残る・ADR 0059 の決定 2）。
class Win32SessionAdapter final : public application::SessionPort
{
  public:
    Win32SessionAdapter(application::FilePort &files,
                        std::expected<SessionPaths, application::SessionFailure> paths);
    [[nodiscard]] std::expected<std::optional<application::Session>, application::SessionFailure>
    read() override;
    [[nodiscard]] std::expected<void, application::SessionFailure>
    write(const application::Session &session) override;

  private:
    application::FilePort &files_;
    std::expected<SessionPaths, application::SessionFailure> paths_;
};
} // namespace nenenib::adapters::win32
