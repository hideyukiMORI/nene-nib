#pragma once

#include <cstdint>

namespace nenenib::core
{
// Markdown の画像の参照先を「出さない」理由（施主決定 D79・ADR 0155 の追補 48・#1381）。どれも期待
// される結果で、画像は今の「[画像: alt]」の文字のまま（知らせは出さない）。値が増えたら switch が落
// ちる（CPP-002）。
// - empty: 参照先が空（`![a]()`・`#frag` だけ）。
// - remote: http / https（ネットの画像は取りに行かない）。
// - data: data: の URI（後続）。
// - unknown_scheme: ほかの scheme（`ftp:` `mailto:` など）。
// - network: UNC（`\\server\share`）・`//host`・`file://host/`・文書がネットワークの場所にある相対
//   参照（ローカルのファイルだけを読む）。
// - no_base: 相対参照で、文書に場所が無い（無題）か、場所がドライブの絶対パスでない。
// - malformed: 読めない参照（ドライブの相対 `C:a.png`・file: の URL の中がドライブの絶対パスでな
//   い・解いた経路が UTF-8 でない・制御文字を含む）。
enum class ImageReferenceRefusal : std::uint8_t
{
    empty,
    remote,
    data,
    unknown_scheme,
    network,
    no_base,
    malformed
};
} // namespace nenenib::core
