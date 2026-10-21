#pragma once

#include "HtmlTag.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// markdown_from_html の字句器が渡す開き・閉じ・文字を Markdown の文字列に書く
// （ADR 0157 の決定 6・7）。積み（開いた要素・引用の深さ・リストの番号・リンクの行き先）は
// ここの中だけに持つ。
// 改行と行頭の印は次の中身が来たときに書く（空の要素は何も残さない）。開いた記号（`**` `[` `#`）も
// 中身が来るまで保留し、中身の無いまま閉じたら捨てる。閉じない要素は finished が閉じる（決定 7）。
class HtmlMarkdownWriter final
{
  public:
    // attributes は開きタグの名前の後ろ（`href="…"` など・解く前の綴り）。
    void open(HtmlTag tag, std::string_view attributes);
    // 開いた要素の内でいちばん内側の同じ値まで閉じる。開いていない値は何もしない。
    void close(HtmlTag tag);
    // 実体参照を解いた後の文字。pre の外では空白の並びを 1 つに畳む。
    void text(std::string_view decoded);

    [[nodiscard]] std::string finished() &&;

  private:
    void open_block(HtmlTag tag);
    void open_inline(HtmlTag tag, std::string_view attributes);
    void open_list(HtmlTag tag);
    void open_item();
    void open_code(std::string_view attributes);
    void write_image(std::string_view attributes);
    void write_rule();
    void write_line_break();
    void end(HtmlTag tag);
    void end_inline(HtmlTag tag);
    void end_list();
    void end_preformatted();
    void close_mark(std::string_view opening, std::string_view closing);
    void request_breaks(std::size_t count);
    void emit_breaks();
    void begin_content();
    void begin_raw_line();
    void write_prefix();
    void write_fence(std::string_view language);
    void flowing_text(std::string_view decoded);
    void raw_text(std::string_view decoded);
    void raw_character(char character);

    std::string out_;
    std::string pending_marks_;
    std::optional<std::string> list_marker_;
    std::vector<HtmlTag> open_;
    // 番号つきのリストは次の番号、箇条書きは値なし。
    std::vector<std::optional<std::size_t>> lists_;
    // a の行き先。href の無い a は値なし（記法を出さない）。
    std::vector<std::optional<std::string>> links_;
    std::size_t quote_depth_ = 0;
    std::size_t pending_breaks_ = 0;
    std::size_t blank_quote_depth_ = 0;
    std::size_t preformatted_depth_ = 0;
    bool fence_pending_ = false;
    bool skip_newline_ = false;
    bool line_start_ = true;
    bool pending_space_ = false;
};
} // namespace nenenib::core
