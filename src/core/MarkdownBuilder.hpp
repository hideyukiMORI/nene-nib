#pragma once

#include "DisplayLanguage.hpp"
#include "HeadingSource.hpp"
#include "Language.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownImage.hpp"
#include "MarkdownStop.hpp"
#include "MarkdownTask.hpp"
#include "PreviewAlert.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewCodePiece.hpp"
#include "PreviewStyle.hpp"
#include "PreviewTableAlign.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
// md4c の enter / leave の積みを平らな leaf ブロックの列に畳む
// （ADR 0151 の決定 7・8）。md4c の型を知らない（md4c.h を読むのは
// MarkdownParse.cpp だけ・決定 5）。積み（引用の深さ・リストの番号・
// 開いた装飾）はここの中だけに持ち、leaf ごとに 1 要素を出す。
// 項目（LI）の直下の文字は暗黙の leaf として list_item に、
// 項目の最初の段落も list_item に畳み、項目の記号はその最初の要素が持つ。
class MarkdownBuilder final
{
  public:
    // input は本文の全体。text の source がこの中を指すときだけ位置に使う。
    MarkdownBuilder(std::string_view input, const MarkdownStop &stop,
                    DisplayLanguage language) noexcept;

    // 述語に問う。一度 true を聞いたら以後は問わずに true を返し続ける（打ち切りを取り消さない）。
    [[nodiscard]] bool should_stop();
    [[nodiscard]] bool stopped() const noexcept;
    // コールバックか述語が std::bad_alloc を投げたことを覚える（#817）。
    void exhaust() noexcept;
    [[nodiscard]] bool exhausted() const noexcept;

    // 本文の先頭が `---` だけの行で、後ろに `---` か `...` だけの行があれば、
    // その間を front_matter の 1 要素にして、閉じる行の次の行頭を返す
    // （md4c にはそこから先を渡す・#1840）。無ければ 0 で、何も足さない。
    // md4c の要素の位置は本文を指す pointer から引くのでずれない。
    // task の印の位置（md4c に渡した本文の中の位置）は enter_item が
    // この値を足す。最初のコールバックの前に 1 回だけ呼ぶ。
    [[nodiscard]] std::size_t front_matter();

    void enter_quote();
    void leave_quote();
    // numbering は番号つきの最初の番号と区切り（'.' か ')'・#1840）。箇条書きは nullopt。
    void enter_list(std::optional<std::pair<std::uint64_t, char>> numbering);
    void leave_list();
    // task はタスクリストの項目のチェックボックス（ADR 0155 の追補 22・ほかは nullopt）。項目の
    // 記号と同じく、項目の最初の要素が持つ。
    void enter_item(std::optional<MarkdownTask> task);
    void leave_item();
    void rule();
    // 表（#1848・D103）。表の前の項目の記号は空の要素で出し、行の番号を 0 から数え直す。行ごとに
    // 要素 table_row を 1 つ開き（閉じるのは close_leaf）、升ごとに始まりと揃えを足す（升の間は
    // Tab 1 つ）。header は升が見出しの升（TH）か。
    void enter_table();
    // 表を閉じる。表の行の全部に表の要約（preview_table_digest）を置き、表の通し番号を進める。
    void leave_table();
    void enter_row();
    void enter_cell(PreviewTableAlign align, bool header);
    void open_leaf(PreviewBlockKind kind, std::uint8_t level);
    // 囲みのコードを開く（#1858）。language は info string の最初の語から引いた言語
    // （preview_code_language・無ければ nullopt で 1 色）。閉じる close_leaf が 1
    // 回だけ色の区間を作る。
    void open_code(std::optional<Language> language);
    void close_leaf();
    // style が nullopt の装飾（数式など）は積みの対だけを保つ。
    void enter_span(std::optional<PreviewStyle> style);
    // 打ち消し（md4c の DEL）を開く。打ち消しは `~~` だけで、開く記号が `~` 1 つなら装飾を付けず、
    // 開く `~` と閉じる `~` を文字として元の位置に戻す（日本語の範囲の `~`・#1840 のレビュー）。
    // 記号の数は span の中の最初の文字の本文の位置から前を読んで決める。
    void enter_strike();
    // 画像の装飾を開き、画像の節を覚える（ADR 0155 の追補 47）。source と title は実体参照を解いた
    // md4c の属性。閉じる leave_span が alt と範囲の終わりを書く。
    void enter_image(std::string source, std::string title);
    // リンクの装飾を開き、宛先を覚える（ADR 0151 の追補 2026-11-19・#1869）。
    // target は実体参照とエスケープを解いた md4c の href。
    // 閉じる close_leaf が装飾の範囲と合わせて要素の links に置く。
    void enter_link(std::string target);
    void leave_span();
    // shown を要素の文字に足す。source が本文の中を指せば、要素の最初の位置と直前の終わりに使う。
    // 見出しの中なら名札に数える文字にも足す（ADR 0151 の段 L3）。
    void text(std::string_view shown, std::string_view source);
    // 生の HTML（md4c の MD_TEXT_HTML）を text と同じく要素の文字に足す。名札には数えない
    // （GitHub はタグを見出しの文字に数えない・ADR 0151 の段 L3）。
    void html(std::string_view shown, std::string_view source);

    // 開いている要素を閉じ、要素の列と画像の節と見出しの名札の列（heading_slugs の 1 回）を返す。
    [[nodiscard]] MarkdownDocument finished() &&;

  private:
    // text と html が共に通る、要素の文字に足す 1 本。
    void append_shown(std::string_view shown, std::string_view source);
    [[nodiscard]] std::optional<std::size_t> located(std::string_view source) const;
    [[nodiscard]] std::size_t unlocated_begin() const noexcept;
    [[nodiscard]] PreviewBlock started(PreviewBlockKind kind, std::uint8_t level);
    [[nodiscard]] PreviewBlock &opened(PreviewBlockKind kind, std::uint8_t level);
    PreviewBlock &ensure_leaf();
    void close_implicit();
    void flush_marker();
    // 開いている画像の節のいちばん内側を、要素の text の今の終わりで閉じる。closed は閉じの「]」を
    // 足した後か。
    void finish_image(const PreviewBlock &block, bool closed);
    // 開いたままの打ち消しの記号の数を、span の中の最初の文字の本文の位置 at から決める。
    void settle_strikes(std::optional<std::size_t> at);
    // 装飾 span の打ち消しを `~` 1 つと決め、要素の text の begin に開く `~` を戻す。
    void restore_opening(std::size_t span, std::size_t begin);
    // 閉じる要素に alert の種類を置く。いちばん外の引用の最初の要素なら、その段落の 1 行目が
    // alert の印の行かを見て、印を種類の名前に置き換える（#1848）。
    void mark_alert(PreviewBlock &block);
    // 段落 block の 1 行目が alert の印の行なら、印を種類の名前に置き換えてその種類を返す。
    [[nodiscard]] std::optional<PreviewAlert> titled_alert(PreviewBlock &block);
    // 囲みのコードの要素に足した文字 shown（本文の位置 at）から、切れ（preview_code_piece_lines
    // 行ごと）の最初の行の本文の位置を覚える（#1863）。
    void note_code_text(std::string_view shown, std::optional<std::size_t> at);
    // 囲みのコードの次の行へ進む（切れの最初の行なら、その行頭の見込みを覚える）。
    void next_code_line();
    // 今の行 code_line_ の行頭の本文の位置の見込み（最後に位置を持った行の終わりから行を数えて
    // 進める・空の行は位置を持たないので）。前に位置を持った行が無ければ nullopt。
    [[nodiscard]] std::optional<std::size_t> code_line_start() const noexcept;
    // 閉じた囲みのコードの要素を、切れの列にして色の区間を置いて足す（#1863）。
    void place_code(PreviewBlock block);
    [[nodiscard]] Offset piece_begin(std::size_t piece) const;

    std::string_view input_;
    const MarkdownStop *stop_;
    // 画像の代わりの文字の枠の前と後ろ（表の文 preview_image_alt を置き場で割った静的な文）。
    std::string_view image_opening_;
    std::string_view image_closing_;
    std::vector<PreviewBlock> blocks_;
    std::optional<PreviewBlock> leaf_;
    std::optional<std::size_t> leaf_begin_;
    bool implicit_leaf_ = false;
    std::optional<std::string> marker_;
    std::optional<MarkdownTask> task_;
    std::size_t quote_depth_ = 0;
    std::vector<std::optional<std::pair<std::uint64_t, char>>> lists_;
    // md4c に渡した本文の先頭の、本文の中の位置（front_matter の後ろ・無ければ 0）。
    std::size_t parsed_from_ = 0;
    std::vector<std::optional<std::size_t>> spans_;
    // 記号の数がまだ決まっていない打ち消しの装飾の番号と、要素の text の中の始まり。
    std::vector<std::pair<std::size_t, std::size_t>> pending_strikes_;
    // 記号が `~` 1 つと決まった打ち消しの装飾の番号（要素を閉じるときに装飾から外す）。
    std::vector<std::size_t> single_strikes_;
    std::vector<MarkdownImage> images_;
    // 開いている画像の節の images_ の番号（入れ子は外側が先）。
    std::vector<std::size_t> open_images_;
    // 開いている要素のリンクの装飾の番号と宛先（開いた順）。
    std::vector<std::pair<std::size_t, std::string>> link_targets_;
    // 開いている見出しの名札に数える文字と、閉じた見出しの名札の元の列（ADR 0151 の段 L3）。
    std::string heading_text_;
    std::vector<HeadingSource> heading_sources_;
    std::optional<std::size_t> last_end_;
    // 今の表の次の行の番号。
    std::uint32_t table_row_ = 0;
    // 今の表の最初の行の要素の番号。
    std::size_t table_start_ = 0;
    // 今の表の通し番号（閉じた表の数・#1883）。
    std::uint32_t tables_ = 0;
    // いちばん外の引用（リストの外）の最初の要素の番号と、その引用が alert ならその種類。
    std::optional<std::size_t> alert_start_;
    std::optional<PreviewAlert> alert_;
    // 開いている囲みのコードの言語（open_code・閉じたら空）。
    std::optional<Language> code_language_;
    // 開いている囲みのコードの今の行の番号（0 から）・今の行が本文の位置を持ったか・最後に位置を
    // 持った行の番号・2 つ目からの切れの最初の行の本文の位置（#1863）。
    std::size_t code_line_ = 0;
    bool code_line_located_ = false;
    std::optional<std::size_t> code_located_line_;
    std::vector<std::optional<std::size_t>> code_piece_begins_;
    bool stopped_ = false;
    bool exhausted_ = false;
};
} // namespace nenenib::core
