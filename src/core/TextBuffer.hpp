#pragma once

#include "AddChunk.hpp"
#include "DetectedText.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Piece.hpp"
#include "TextFailure.hpp"
#include "TextPosition.hpp"
#include "simd/AsciiNewlineScan.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 本文の正本（piece table・ADR 0009 の決定 1）。公開状態は不変で、編集は次の本文を返す（ARC-005）。
// original（読んだ本文）と add（入力）の 2 本は shared_ptr で共有し、複製するのは piece の列だけ。
// add は固定長の chunk の列で、末尾の chunk を伸ばすのはその先端を知っている値だけ（ADR 0044）。
// 改行の索引は original と各 chunk に 1 本ずつで、piece はその窓だけを持つ（ADR 0047）。
// 生 UTF-8 と判定済みの所有値を受け取り、同じ構築経路で正しい UTF-8 を保つ（ADR 0080）。
class TextBuffer final
{
  public:
    [[nodiscard]] static TextBuffer empty();
    [[nodiscard]] static std::expected<TextBuffer, TextFailure> from_utf8(std::string_view text);
    [[nodiscard]] static std::expected<TextBuffer, TextFailure> from_utf8(DetectedText text);
    // 改行の索引を合成ルートが選んだ走査で作る（ADR 0006 の決定 2）。
    [[nodiscard]] static std::expected<TextBuffer, TextFailure>
    from_utf8(DetectedText text, simd::AsciiNewlineScan scan);

    [[nodiscard]] TextBuffer insert(Offset at, std::string_view text) const;
    [[nodiscard]] TextBuffer erase(Offset begin, Offset end) const;
    // 編集の原始で、insert / erase はその薄い口。application からも直接呼ぶ（ARC-001）。
    // 範囲は本文の中へ丸める（begin は size まで、end は begin と size の間）。
    [[nodiscard]] TextBuffer replaced(Offset begin, Offset end, std::string_view text) const;

    // 本文の改行の形（ADR 0036 の決定 1）。読んだときに 1 度だけ判別し、編集では変わらない。
    [[nodiscard]] LineEnding line_ending() const noexcept;

    [[nodiscard]] std::size_t size_bytes() const noexcept;
    [[nodiscard]] std::size_t line_count() const noexcept;
    [[nodiscard]] std::size_t piece_count() const noexcept;

    // 本文順の借用。列だけを所有し、文字はこの値の生存中だけ有効（ADR 0119）。
    // 同期処理でだけ使い、この値の破棄・代入・移動を越えて保持しない。
    [[nodiscard]] std::vector<std::string_view> segments() const &;
    std::vector<std::string_view> segments() const && = delete;

    [[nodiscard]] std::string text() const;
    [[nodiscard]] std::string text_range(Offset begin, Offset end) const;
    // 行の文字列。piece をまたぐので string_view では返せない（ADR 0009）。末尾の改行は含まない。
    [[nodiscard]] std::string line_text(LineNumber line) const;

    // 行の先頭と、行の内容の終わりのバイト位置。'\n' の直前の '\r' を改行の一部として外すのは
    // 改行の形が CRLF のときだけで、LF の本文では '\r' は 1 文字である（ADR 0036 の決定 2）。
    [[nodiscard]] Offset line_start(LineNumber line) const noexcept;
    [[nodiscard]] Offset line_end(LineNumber line) const noexcept;
    // 次の行の先頭（最終行では本文の末尾）。改行そのものを含む半開区間の端。
    [[nodiscard]] Offset line_terminator_end(LineNumber line) const noexcept;

    [[nodiscard]] Offset offset_of(const TextPosition &position) const;
    [[nodiscard]] TextPosition position_of(Offset at) const;

  private:
    [[nodiscard]] static TextBuffer from_validated_utf8(std::string text,
                                                        simd::AsciiNewlineScan scan);

    using Buffer = std::shared_ptr<const std::string>;
    using Chunks = std::vector<std::shared_ptr<AddChunk>>;
    using Index = std::shared_ptr<const NewlineIndex>;

    [[nodiscard]] static Piece appended(Chunks &chunks, std::size_t &fill, std::string_view text);

    TextBuffer(Buffer original, Index original_newlines, std::vector<Piece> pieces,
               LineEnding ending);
    [[nodiscard]] std::string_view view_of(const Piece &piece) const noexcept;
    [[nodiscard]] char byte_at(Offset at) const noexcept;
    [[nodiscard]] const std::string &buffer_of(const Piece &piece) const noexcept;
    // piece が指すバッファの改行の索引。窓 [newline_begin, newline_end) の添字はこの列の中を指す。
    [[nodiscard]] const NewlineIndex &index_of(const Piece &piece) const noexcept;
    void collect(std::vector<Piece> &out, std::size_t from, std::size_t to) const;
    [[nodiscard]] Piece clipped(const Piece &piece, std::size_t from, std::size_t length) const;
    [[nodiscard]] std::size_t newline_offset(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t newlines_before(std::size_t at) const noexcept;

    Buffer original_;
    // original の '\n' のbit索引。from_utf8 で 1 度だけ作り、値のあいだで共有する（ADR 0121）。
    Index original_newlines_;
    Chunks add_;
    // 末尾の chunk の、この値が知っている書き込み済みの長さ（ADR 0044 の決定 2）。
    std::size_t add_fill_;
    std::vector<Piece> pieces_;
    std::size_t size_bytes_;
    std::size_t newline_count_;
    LineEnding ending_;
};
} // namespace nenenib::core
