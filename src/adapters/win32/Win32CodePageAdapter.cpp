#include "Win32CodePageAdapter.hpp"

#include <cstddef>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::CodePageFailure;
// 日本語 Windows の従来の文字集合。CP932 の表は OS のものを使う（ADR 0010 の決定 2）。
constexpr UINT code_page_932 = 932;

[[nodiscard]] std::expected<std::wstring, Failure> widen(UINT code_page, std::string_view text)
{
    if (text.empty())
    {
        return std::wstring{};
    }
    const auto bytes = static_cast<int>(text.size());
    const int length =
        MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, text.data(), bytes, nullptr, 0);
    if (length <= 0)
    {
        return std::unexpected(Failure::undecodable);
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, text.data(), bytes, wide.data(),
                            length) != length)
    {
        return std::unexpected(Failure::undecodable);
    }
    return wide;
}
} // namespace

std::expected<std::string, Failure> Win32CodePageAdapter::to_utf8(std::string_view cp932)
{
    const auto wide = widen(code_page_932, cp932);
    if (!wide)
    {
        return std::unexpected(wide.error());
    }
    const auto units = static_cast<int>(wide.value().size());
    if (units == 0)
    {
        return std::string{};
    }
    const int bytes =
        WideCharToMultiByte(CP_UTF8, 0, wide.value().data(), units, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
    {
        return std::unexpected(Failure::undecodable);
    }
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.value().data(), units, utf8.data(), bytes, nullptr,
                        nullptr);
    return utf8;
}

std::expected<std::string, Failure> Win32CodePageAdapter::from_utf8(std::string_view utf8)
{
    const auto wide = widen(CP_UTF8, utf8);
    if (!wide)
    {
        return std::unexpected(Failure::unencodable);
    }
    const auto units = static_cast<int>(wide.value().size());
    if (units == 0)
    {
        return std::string{};
    }
    // WC_NO_BEST_FIT_CHARS: 似た字で埋めさせない。埋めようとしたことは used が教える（決定 2）。
    BOOL used = FALSE;
    const int bytes = WideCharToMultiByte(code_page_932, WC_NO_BEST_FIT_CHARS, wide.value().data(),
                                          units, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
    {
        return std::unexpected(Failure::unencodable);
    }
    std::string cp932(static_cast<std::size_t>(bytes), '\0');
    const char replacement = '?';
    WideCharToMultiByte(code_page_932, WC_NO_BEST_FIT_CHARS, wide.value().data(), units,
                        cp932.data(), bytes, &replacement, &used);
    if (used != FALSE)
    {
        return std::unexpected(Failure::unencodable);
    }
    return cp932;
}
} // namespace nenenib::adapters::win32
