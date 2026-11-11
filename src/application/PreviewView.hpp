#pragma once

#include "DisplayText.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewImages.hpp"
#include "PreviewSelection.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace nenenib::application
{
// もう 1 つの枠に置く Markdown のプレビュー（ADR 0155 の決定 1・2）。文書の値ではなく窓の値で、操作
// 中の文書の本文を映す。document は見せている結果（最初は空・差し替わるまで前の結果を出す）、ticket
// は最後に出した券（0 は未要求）、requested_revision は最後に要求した本文の改訂番号、seen_revision
// は前の PreviewTick で見た改訂番号（決定 8）。anchor は枠の上端に来るブロックを引く本文のバイト位
// 置で、follows の間は frame を作るときに本文の先頭行の行頭を使う（決定 7）。notice は 16 MiB 超と
// 解析の失敗の 1 行（決定 3・6）。selection は全面プレビューの選択（改訂 18・全面の間だけ持ち、側へ
// 移すときと結果が差し替わるときに捨てる）。images は document の画像の状態（ADR 0155 の追補 50・
// document に画像の節が無ければ null・文書が替わると捨てる）。どの欄も単独で妥当な値なので公開
// aggregate （CPP-003）。比較は非メンバーで、document と images は指す先の同一性で比べる。
struct PreviewView
{
    std::shared_ptr<const core::MarkdownDocument> document;
    std::uint64_t ticket = 0;
    std::size_t requested_revision = 0;
    std::size_t seen_revision = 0;
    core::Offset anchor{0};
    bool follows = true;
    std::optional<core::DisplayText> notice;
    std::optional<core::PreviewSelection> selection;
    std::shared_ptr<const PreviewImages> images = nullptr;
};

[[nodiscard]] inline bool same_notice(const std::optional<core::DisplayText> &left,
                                      const std::optional<core::DisplayText> &right) noexcept
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    return !left.has_value() || left.value().text() == right.value().text();
}

[[nodiscard]] inline bool operator==(const PreviewView &left, const PreviewView &right) noexcept
{
    return left.document == right.document && left.ticket == right.ticket &&
           left.requested_revision == right.requested_revision &&
           left.seen_revision == right.seen_revision && left.anchor == right.anchor &&
           left.follows == right.follows && same_notice(left.notice, right.notice) &&
           left.selection == right.selection && left.images == right.images;
}
} // namespace nenenib::application
