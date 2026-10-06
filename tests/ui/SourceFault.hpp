#pragma once
namespace nenenib::tests::ui
{
// 替え玉の source が字体選択の保持の前提をどう崩すか（ADR 0071 の決定 2）。
enum class SourceFault
{
    none,
    hidden_tail,      // 要求の末尾の後ろにまだ文字が続く
    hidden_prefix,    // 位置 0 の前にも文字がある
    broken_text,      // 本文を読めない
    broken_extension, // 拡張の QueryInterface が E_NOINTERFACE 以外で失敗する
};
} // namespace nenenib::tests::ui
