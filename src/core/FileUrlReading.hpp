#pragma once

#include <cstdint>

namespace nenenib::core
{
// `file:` の URL を読む 2 つの呼び出し元（ARC-012・#1825 のレビュー R2）。規則の違いは
// file_url_path の 1 か所に書く。
// - image_reference: Markdown の画像の参照（ADR 0155 の追補 48）。host は空か localhost だけ
//   （ほかは network）・根より上へ出る `..` は根で止める・ドライブ文字は綴りのまま。
// - language_server: 言語サーバーの答えの URI（ADR 0190 の決定 4）。host のある形は UNC として受け
//   （device の名前空間は断る）・畳めない `..` は断る・ドライブ文字は大文字にそろえる。
enum class FileUrlReading : std::uint8_t
{
    image_reference,
    language_server
};
} // namespace nenenib::core
