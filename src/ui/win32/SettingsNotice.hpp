#pragma once

#include "SettingsFailure.hpp"

#include <utility>

namespace nenenib::ui::win32
{
[[nodiscard]] inline const wchar_t *settings_notice(application::SettingsFailure failure)
{
    using Failure = application::SettingsFailure;
    switch (failure)
    {
    case Failure::location_unavailable:
        return L"設定の保存先を取得できませんでした。設定は保存されません。";
    case Failure::unreadable:
        return L"設定ファイルを読めませんでした。元の設定は保持します。";
    case Failure::too_large:
        return L"設定ファイルが上限の 4096 バイトを超えています。元の設定は保持します。";
    case Failure::malformed:
        return L"設定ファイルの形式が正しくありません。元の設定は保持します。";
    case Failure::unsupported_version:
        return L"設定ファイルの版に対応していません。元の設定は保持します。";
    case Failure::unknown_theme:
        return L"設定ファイルのテーマ名が見つかりません。元の設定は保持します。";
    case Failure::invalid_font_family:
        return L"設定ファイルのフォント名が正しくありません。元の設定は保持します。";
    case Failure::invalid_font_size:
        return L"設定ファイルの文字サイズは 8〜40 pt で指定してください。元の設定は保持します。";
    case Failure::unwritable:
        return L"設定を保存できませんでした。別の起動で使用中、または書き込みが許可されていません"
               L"。";
    case Failure::changed_externally:
        return L"別の処理で設定ファイルが変更されました。上書きせずに保持します。再起動で読み直して"
               L"ください。";
    case Failure::not_loaded:
        return L"設定を読み込めていないため、保存しませんでした。";
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
