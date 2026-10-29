#pragma once

#include "AddChunk.hpp"
#include "DetectedText.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Piece.hpp"
#include "TextFailure.hpp"
#include "TextPosition.hpp"
#include "VirtualColumnIndex.hpp"
#include "VirtualColumnsFailure.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
// 本文の正本（piece table・ADR 0009 の決定 1）。公開状態は不変で、編集は次の本文を返す（ARC-005）。
// original（読んだ本文）と add（入力）の 2 本は shared_ptr で共有し、複製するのは piece の列だけ。
// add は固定長の chunk の列で、末尾の chunk を伸ばすのはその先端を知っている値だけ（ADR 0044）。
// 改行の索引は original と各 chunk に 1 本ずつで、piece はその窓だけを持つ（ADR 0047）。
// 仮想桁の索引は要る文書だけが本文全体に 1 本持ち、編集は触れた塊だけを作り直す（ADR 0140 の決定
// 4）。 生 UTF-8 と判定済みの所有値を受け取り、同じ構築経路で正しい UTF-8 を保つ（ADR 0080）。
class TextBuffer final
{
  public:
    [[nodiscard]] static TextBuffer empty();
    [[nodiscard]] static std::expected<TextBuffer, TextFailure> from_utf8(std::string_view text);
    [[nodiscard]] static std::expected<TextBuffer, TextFailure> from_utf8(DetectedText text);

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
    // 最長の行のバイト数（LF を含まず、CR は数える）。改行の索引の 512 byte の塊の要約を
    // piece の順に畳み、piece の端の塊だけを bit から数える。本文の文字は読まない（ADR 0047 /
    // 0121・Issue #553）。O(塊)。
    [[nodiscard]] std::size_t longest_line_bytes() const noexcept;

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

    // 仮想桁の索引を持つ本文は索引の序数で O(塊) に解き、持たない本文は行頭から歩く（ADR 0139 の
    // 決定 6）。TextPosition の桁はコードポイントの序数＋1 で、索引の序数と同じ単位。
    [[nodiscard]] Offset offset_of(const TextPosition &position) const;
    [[nodiscard]] TextPosition position_of(Offset at) const;
    // 上の 2 つと同じ答えに、答えるために文字を読んだ本文のバイト数を添える（契約が O(塊) を
    // 確かめる読む口）。position_of / offset_of はこの答えの first だけを返す。
    [[nodiscard]] std::pair<Offset, std::size_t> offset_read(const TextPosition &position) const;
    [[nodiscard]] std::pair<TextPosition, std::size_t> position_read(Offset at) const;

    // 仮想桁の索引（ADR 0140 の決定 4）。要る文書だけが持ち、既定では持たない。
    // with_virtual_columns は索引を一から作って持つ写しを返す（持っていればそのまま）。持つ本文の
    // replaced は触れた塊だけを作り直し、持たない本文の replaced は索引に触れない。
    // 索引は改行の索引と同じく不変の値で、値のあいだで共有する。問い合わせにはこの本文を渡す。
    [[nodiscard]] TextBuffer with_virtual_columns() const;
    [[nodiscard]] bool has_virtual_columns() const noexcept;
    // 持たない本文に問うのは呼び出し側の誤りで、not_held を返す。参照はこの値の生存中だけ有効。
    [[nodiscard]] std::expected<std::reference_wrapper<const VirtualColumnIndex>,
                                VirtualColumnsFailure>
    virtual_columns() const &;
    std::expected<std::reference_wrapper<const VirtualColumnIndex>, VirtualColumnsFailure>
    virtual_columns() const && = delete;
    // この値を作るときに読み直した仮想桁の塊の数。索引を持たなければ 0。
    [[nodiscard]] std::size_t rebuilt_column_chunks() const noexcept;

  private:
    // skip は original の先頭で本文に入れないバイト数（UTF-8 の BOM の 3・ADR 0159 の決定 8）。列は
    // 消さずに最初の piece の開始をずらす。
    [[nodiscard]] static TextBuffer from_validated_utf8(std::string text, std::size_t skip = 0);

    using Buffer = std::shared_ptr<const std::string>;
    using Chunks = std::vector<std::shared_ptr<AddChunk>>;
    using Index = std::shared_ptr<const NewlineIndex>;
    using Columns = std::shared_ptr<const VirtualColumnIndex>;

    [[nodiscard]] static Piece appended(Chunks &chunks, std::size_t &fill, std::string_view text);

    TextBuffer(Buffer original, Index original_newlines, Columns columns, std::vector<Piece> pieces,
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
    // 本文全体の仮想桁の部分和。持たない本文は null（ADR 0140 の決定 4）。
    Columns columns_;
    // この値を作るときに読み直した仮想桁の塊の数（rebuilt_column_chunks）。
    std::size_t column_rebuilds_;
    Chunks add_;
    // 末尾の chunk の、この値が知っている書き込み済みの長さ（ADR 0044 の決定 2）。
    std::size_t add_fill_;
    std::vector<Piece> pieces_;
    std::size_t size_bytes_;
    std::size_t newline_count_;
    LineEnding ending_;
};
} // namespace nenenib::core
