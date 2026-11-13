#include "FileFailureText.hpp"

#include "DocumentByteLimit.hpp"
#include "WideUiText.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t mebibyte = std::size_t{1024} * 1024U;
constexpr std::size_t gibibyte = mebibyte * 1024U;
// 文は「1 GB」「64 MiB」の形なので、上限が単位で割り切れることを守る。
static_assert(core::maximum_document_bytes % gibibyte == 0);
static_assert(core::maximum_decoded_document_bytes % mebibyte == 0);

// 単位つきの量は「数 ＋ 単位」の 1 つの置き場に入れる（decisions の 4）。開く上限の単位は今の
// 製品の文のまま GB と書く。
[[nodiscard]] std::string open_limit()
{
    return std::to_string(core::maximum_document_bytes / gibibyte) + " GB";
}

[[nodiscard]] std::string decode_limit()
{
    return std::to_string(core::maximum_decoded_document_bytes / mebibyte) + " MiB";
}
} // namespace

core::UiMessage file_failure_message(application::FileFailure failure) noexcept
{
    using Failure = application::FileFailure;
    using core::UiMessage;
    switch (failure)
    {
    case Failure::not_found:
        return UiMessage::dialog_file_not_found;
    case Failure::access_denied:
        return UiMessage::dialog_file_access_denied;
    case Failure::unreadable:
        return UiMessage::dialog_file_unreadable;
    case Failure::unwritable:
        return UiMessage::dialog_file_unwritable;
    case Failure::already_exists:
        return UiMessage::dialog_file_exists;
    case Failure::too_large:
        return UiMessage::dialog_file_too_large;
    case Failure::too_large_to_decode:
        return UiMessage::dialog_file_too_large_to_decode;
    case Failure::undecodable:
        return UiMessage::dialog_file_undetected_encoding;
    case Failure::unsupported_utf32:
        return UiMessage::dialog_file_utf32;
    case Failure::unencodable:
        return UiMessage::dialog_file_unencodable;
    case Failure::insufficient_memory:
        return UiMessage::dialog_file_out_of_memory;
    }
    std::unreachable();
}

std::wstring file_failure_text(application::FileFailure failure, core::DisplayLanguage language)
{
    const auto message = file_failure_message(failure);
    switch (failure)
    {
    case application::FileFailure::too_large:
    {
        const std::string limit = open_limit();
        return wide_ui_text_filled(message, language, {limit});
    }
    case application::FileFailure::too_large_to_decode:
    {
        const std::string limit = decode_limit();
        return wide_ui_text_filled(message, language, {limit});
    }
    case application::FileFailure::not_found:
    case application::FileFailure::access_denied:
    case application::FileFailure::unreadable:
    case application::FileFailure::unwritable:
    case application::FileFailure::already_exists:
    case application::FileFailure::undecodable:
    case application::FileFailure::unsupported_utf32:
    case application::FileFailure::unencodable:
    case application::FileFailure::insufficient_memory:
        return wide_ui_text(message, language);
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
