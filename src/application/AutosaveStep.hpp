#pragma once

namespace nenenib::application
{
// 1 回の AutosaveTick でタブごとに決める退避の仕事（ADR 0137 の決定 2）。閉じた選択肢で、増えたら
// switch が落ちる（CPP-002）。
enum class AutosaveStep
{
    // 未保存で、前回の退避から改訂番号が変わった。本文を書く。
    store,
    // 保存済みになった・閉じた・まだ読んでいないタブに退避が残っている。消す。
    discard,
    // 何もしない。
    keep,
    // 未保存で変わったが、本文が core::maximum_decoded_document_bytes を超えるので書かない（ADR
    // 0159 の決定 7・L-4）。知らせは文書ごとに 1 回。
    oversized,
};
} // namespace nenenib::application
