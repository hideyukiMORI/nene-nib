#pragma once

#include "DisplayLanguage.hpp"
#include "UiLanguagePort.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::tests
{
// OS の表示言語の替え玉（ADR 0123 の追補 2026-11-13 の決定 2）。OS を読まずに試験が置いた値を返し、
// 呼ばれた回数を数える。既定は日本語（今までの試験の表示が変わらない）。
class ScriptedUiLanguage final : public nenenib::application::UiLanguagePort
{
  public:
    explicit ScriptedUiLanguage(std::optional<nenenib::core::DisplayLanguage> value =
                                    nenenib::core::DisplayLanguage::japanese) noexcept
        : value_(value)
    {
    }

    [[nodiscard]] std::optional<nenenib::core::DisplayLanguage> observe() override
    {
        ++calls_;
        return value_;
    }

    void set(std::optional<nenenib::core::DisplayLanguage> value) noexcept
    {
        value_ = value;
    }

    [[nodiscard]] std::size_t calls() const noexcept
    {
        return calls_;
    }

  private:
    std::optional<nenenib::core::DisplayLanguage> value_;
    std::size_t calls_ = 0;
};
} // namespace nenenib::tests
