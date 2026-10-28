#pragma once

#include "CodePageFailure.hpp"
#include "CodePagePort.hpp"
#include "DetectedText.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "OpenedDocument.hpp"
#include "SealedBytes.hpp"
#include "TextBuffer.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::application
{
// 判定の失敗のうち UTF-32 だけは名指しで断る。ほかは判別できなかった、のまま（#787）。
[[nodiscard]] FileFailure file_failure_of(core::EncodingFailure failure) noexcept;
[[nodiscard]] FileFailure file_failure_of(CodePageFailure failure) noexcept;
// 判定済みの列を本文へ渡す 1 本。CP932 の変換だけはポートの向こう（ADR 0080）。開く道と
// 自動退避の読み戻しが共用する。
[[nodiscard]] std::expected<core::TextBuffer, FileFailure> decoded_text(CodePagePort &code_pages,
                                                                        core::DetectedText text);
// 封印済みの列を判定して復号した文書（ADR 0159 の決定 2・ADR 0141 の追補 10）。worker の上で
// 塊ごとに読み終えた adapter と、下の read_opened_document が共用する 1 本。
[[nodiscard]] std::expected<OpenedDocument, FileFailure>
opened_document_of(CodePagePort &code_pages, core::SealedBytes &&bytes);
// 読んで判定して復号した文書（ADR 0159 の決定 2）。読みの port の実装（同期に揃える adapter と
// 試験の替え玉）が呼ぶ 1 本で、application の controller は呼ばない。スレッドも時刻も持たない。
[[nodiscard]] std::expected<OpenedDocument, FileFailure>
read_opened_document(FilePort &files, CodePagePort &code_pages, const core::FilePath &path,
                     std::size_t maximum_bytes);
} // namespace nenenib::application
