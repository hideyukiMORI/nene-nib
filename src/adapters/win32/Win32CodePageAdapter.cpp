#include "Win32CodePageAdapter.hpp"

#include "Utf16.hpp"

#include <cstddef>
#include <limits>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::CodePageFailure;
// 日本語 Windows の従来の文字集合。CP932 の表は OS のものを使う（ADR 0010 の決定 2）。
// UTF-8 ↔ UTF-16 は core::Utf16 の純関数で、OS を呼ぶのは CP932 ↔ UTF-16 だけ（Issue #13）。
constexpr UINT code_page_932 = 932;

[[nodiscard]] std::expected<std::wstring, Failure> widen_cp932(std::string_view text)
{
    if (text.empty())
    {
        return std::wstring{};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected(Failure::undecodable);
    }
    const auto bytes = static_cast<int>(text.size());
    std::wstring wide;
    int written = 0;
    // CP932は1〜2bytesからUTF16一単位へ写るので、入力bytesが出力単位の上限（ADR0092）。
    wide.resize_and_overwrite(text.size(),
                              [&](wchar_t *data, std::size_t) noexcept
                              {
                                  written = MultiByteToWideChar(code_page_932, MB_ERR_INVALID_CHARS,
                                                                text.data(), bytes, data, bytes);
                                  return written > 0 && written <= bytes
                                             ? static_cast<std::size_t>(written)
                                             : std::size_t{0};
                              });
    if (written <= 0 || written > bytes)
    {
        return std::unexpected(Failure::undecodable);
    }
    return wide;
}
} // namespace

std::expected<std::string, Failure> Win32CodePageAdapter::to_utf8(std::string_view cp932)
{
    const auto wide = widen_cp932(cp932);
    if (!wide)
    {
        return std::unexpected(wide.error());
    }
    auto utf8 = core::to_utf8(wide.value());
    if (!utf8)
    {
        return std::unexpected(Failure::undecodable);
    }
    return std::move(utf8).value();
}

std::expected<std::string, Failure> Win32CodePageAdapter::from_utf8(std::string_view utf8)
{
    const auto wide = core::to_utf16(utf8);
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
