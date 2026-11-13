#pragma once

#include "DisplayLanguage.hpp"
#include "FontSize.hpp"
#include "SettingsFailure.hpp"
#include "SettingsIssue.hpp"
#include "UiMessage.hpp"
#include "Utf16.hpp"
#include "WideUiText.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::ui::win32
{
// 設定ファイルの上限（バイト）。正本は adapters の Win32SettingsAdapter.cpp の
// maximum_settings_bytes（ui から adapters は見えない）。文の「16 KiB（16384 バイト）」の 2 つの
// 置き場をこの 1 つから埋める（#1019・core へ上げるのは後続）。
inline constexpr std::size_t settings_notice_byte_limit = 16384;
static_assert(settings_notice_byte_limit % 1024U == 0);

[[nodiscard]] inline core::UiMessage
settings_failure_message(application::SettingsFailure failure) noexcept
{
    using Failure = application::SettingsFailure;
    using core::UiMessage;
    switch (failure)
    {
    case Failure::location_unavailable:
        return UiMessage::dialog_settings_location_unavailable;
    case Failure::unreadable:
        return UiMessage::dialog_settings_unreadable;
    case Failure::too_large:
        return UiMessage::dialog_settings_too_large;
    case Failure::malformed:
        return UiMessage::dialog_settings_malformed;
    case Failure::unsupported_version:
        return UiMessage::dialog_settings_unsupported_version;
    case Failure::unknown_theme:
        return UiMessage::dialog_settings_unknown_theme;
    case Failure::invalid_font_family:
        return UiMessage::dialog_settings_invalid_font_family;
    case Failure::invalid_font_size:
        return UiMessage::dialog_settings_invalid_font_size;
    case Failure::unwritable:
        return UiMessage::dialog_settings_unwritable;
    case Failure::changed_externally:
        return UiMessage::dialog_settings_changed_elsewhere;
    case Failure::not_loaded:
        return UiMessage::dialog_settings_not_loaded;
    }
    std::unreachable();
}

// 設定の失敗の箱の本文（ADR 0123 の追補 2026-11-13・#1019）。上限の大きさと字の大きさの範囲は
// 定数から差し込む。
[[nodiscard]] inline std::wstring settings_notice(application::SettingsFailure failure,
                                                  core::DisplayLanguage language)
{
    const auto message = settings_failure_message(failure);
    if (failure == application::SettingsFailure::too_large)
    {
        const std::string size = std::to_string(settings_notice_byte_limit / 1024U) + " KiB";
        const std::string bytes = std::to_string(settings_notice_byte_limit);
        return wide_ui_text_filled(message, language, {size, bytes});
    }
    if (failure == application::SettingsFailure::invalid_font_size)
    {
        const std::string smallest =
            std::to_string(static_cast<int>(core::FontSize::minimum_points));
        const std::string largest =
            std::to_string(static_cast<int>(core::FontSize::maximum_points));
        return wide_ui_text_filled(message, language, {smallest, largest});
    }
    return wide_ui_text(message, language);
}

// テーマの失敗は英語の理由（decisions の 6・今日は訳さない）＋ 改行 ＋ 表の文。
[[nodiscard]] inline std::wstring settings_notice(const core::ThemeLookupFailure &failure,
                                                  core::DisplayLanguage language)
{
    return core::to_utf16(core::theme_failure_message(failure).text()).value() + L"\n" +
           wide_ui_text(core::UiMessage::dialog_settings_theme_kept, language);
}

[[nodiscard]] inline std::wstring settings_notice(const application::SettingsIssue &issue,
                                                  core::DisplayLanguage language)
{
    return std::visit([language](const auto &value) { return settings_notice(value, language); },
                      issue);
}
} // namespace nenenib::ui::win32
