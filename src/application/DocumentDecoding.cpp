#include "DocumentDecoding.hpp"

#include "TextEncoding.hpp"
#include "TextFailure.hpp"

#include <utility>

namespace nenenib::application
{
FileFailure file_failure_of(core::EncodingFailure failure) noexcept
{
    switch (failure)
    {
    case core::EncodingFailure::unsupported_utf32:
        return FileFailure::unsupported_utf32;
    case core::EncodingFailure::undecodable:
    case core::EncodingFailure::odd_utf16_length:
        return FileFailure::undecodable;
    }
    std::unreachable();
}

FileFailure file_failure_of(CodePageFailure failure) noexcept
{
    switch (failure)
    {
    case CodePageFailure::undecodable:
        return FileFailure::undecodable;
    case CodePageFailure::unencodable:
        return FileFailure::unencodable;
    }
    std::unreachable();
}

std::expected<core::TextBuffer, FileFailure> decoded_text(CodePagePort &code_pages,
                                                          core::DetectedText text)
{
    switch (text.encoding())
    {
    case core::TextEncoding::utf8:
    case core::TextEncoding::utf8_bom:
    case core::TextEncoding::utf16_le:
    case core::TextEncoding::utf16_be:
        return core::TextBuffer::from_utf8(std::move(text))
            .transform_error([](core::TextFailure) { return FileFailure::undecodable; });
    case core::TextEncoding::shift_jis:
        break;
    }
    auto converted = code_pages.to_utf8(text.bytes());
    if (!converted)
    {
        return std::unexpected(file_failure_of(converted.error()));
    }
    return core::TextBuffer::from_utf8(converted.value())
        .transform_error([](core::TextFailure) { return FileFailure::undecodable; });
}

// 上限は呼び手（application）が決めてポートへ渡す。読んでから断るのでは大きいファイルを先に抱える。
std::expected<OpenedDocument, FileFailure> read_opened_document(FilePort &files,
                                                                CodePagePort &code_pages,
                                                                const core::FilePath &path,
                                                                std::size_t maximum_bytes)
{
    auto bytes = files.read(path, maximum_bytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    return opened_document_of(code_pages, std::move(bytes).value());
}

std::expected<OpenedDocument, FileFailure> opened_document_of(CodePagePort &code_pages,
                                                              core::SealedBytes &&bytes)
{
    // 封印済みの列をそのまま判定へ移す。複写はしない（ADR 0141 決定 4）。
    auto detected = core::DetectedText::from_sealed(std::move(bytes));
    if (!detected)
    {
        return std::unexpected(file_failure_of(detected.error()));
    }
    const core::TextEncoding encoding = detected.value().encoding();
    auto text = decoded_text(code_pages, std::move(detected).value());
    if (!text)
    {
        return std::unexpected(text.error());
    }
    return OpenedDocument{std::move(text).value(), encoding};
}
} // namespace nenenib::application
