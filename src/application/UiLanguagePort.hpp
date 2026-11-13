#pragma once

#include "DisplayLanguage.hpp"

#include <optional>

namespace nenenib::application
{
// OS の表示言語を観測する唯一の入口（ADR 0123 の追補 2026-11-13 の決定 2・#1019）。読むのは実装
// （src/adapters/win32）だけで（ARC-007）、controller が呼ぶのは起動の 1 回と `:set language=`
// の明示の操作のときだけ。打鍵・描画・タブの切り替え・保存・開くでは呼ばない（契約
// verify_quiet_paths が回数を数える）。列のある言語に写せなければ値なし（解決は core の
// resolved_language が english にする）。
class UiLanguagePort
{
  public:
    UiLanguagePort() = default;
    virtual ~UiLanguagePort() = default;
    UiLanguagePort(const UiLanguagePort &) = delete;
    UiLanguagePort(UiLanguagePort &&) = delete;
    UiLanguagePort &operator=(const UiLanguagePort &) = delete;
    UiLanguagePort &operator=(UiLanguagePort &&) = delete;

    [[nodiscard]] virtual std::optional<core::DisplayLanguage> observe() = 0;
};
} // namespace nenenib::application
