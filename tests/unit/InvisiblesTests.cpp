// scope `--invisibles` の単体テスト（ADR 0193 の決定 1・3・4・5・10・14・15・16 の段
// 2・#1977・#1011 の行末の空白）。種類ごとの区間（`trail` と
// `all`・全角空白・Tab・まとめ）・行ごとの LF / CRLF・ 改行の無い最後の行・lone CR
// に印なし・印の色、`:set list` と種類ごとの `:set`・Ctrl+P の候補・
// 設定の画面の行・機能の表の行、application の frame の欄（オフで空・見えている行だけ・split の
// 両枠・巨大な行と差し込み行で付けない・Vim のモードで隠す最後の改行）。
#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "Column.hpp"
#include "ComposeText.hpp"
#include "DisplayForm.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FeatureCosts.hpp"
#include "FilePath.hpp"
#include "GiantLine.hpp"
#include "InvisibleKind.hpp"
#include "InvisibleSpan.hpp"
#include "Invisibles.hpp"
#include "LineBreakMark.hpp"
#include "LineInvisibles.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "ListSpace.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SettingsChoices.hpp"
#include "SettingsRow.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "ThemeDerivation.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
using core::Column;
using core::InvisibleKind;
using core::InvisibleSpan;
using core::LineBreakMark;
using core::ListSpace;

[[nodiscard]] core::EditorSettings shaped(ListSpace space, bool tab, bool wide)
{
    auto settings = core::default_editor_settings();
    settings.list_space = space;
    settings.list_tab = tab;
    settings.list_wide_space = wide;
    return settings;
}

[[nodiscard]] std::vector<InvisibleSpan> spans_of(std::string_view line,
                                                  const core::EditorSettings &settings)
{
    return core::invisible_spans(line, settings);
}

[[nodiscard]] InvisibleSpan span(std::size_t begin, std::size_t end, InvisibleKind kind)
{
    return InvisibleSpan{Column{begin}, Column{end}, kind};
}

// ---------------------------------------------------------------- 区間（決定 10）

void verify_spans()
{
    const auto trail = shaped(ListSpace::trail, true, true);
    expect(spans_of("a b  ", trail) ==
               std::vector<InvisibleSpan>{span(4, 6, InvisibleKind::trailing_space)},
           "trail marks only the spaces at the end of the line");
    expect(spans_of("a b  ", shaped(ListSpace::all, true, true)) ==
               std::vector<InvisibleSpan>{span(2, 3, InvisibleKind::space),
                                          span(4, 6, InvisibleKind::trailing_space)},
           "all marks every space and keeps the trailing ones strong");
    expect(spans_of("a b  ", shaped(ListSpace::none, true, true)).empty(), "none marks no space");
    expect(spans_of("\tx \t ", trail) ==
               std::vector<InvisibleSpan>{
                   span(1, 2, InvisibleKind::tab), span(3, 4, InvisibleKind::trailing_space),
                   span(4, 5, InvisibleKind::tab), span(5, 6, InvisibleKind::trailing_space)},
           "the trailing run goes on through Tabs like Vim's trail and Tabs are marked");
    expect(spans_of("   ", trail) ==
               std::vector<InvisibleSpan>{span(1, 4, InvisibleKind::trailing_space)},
           "a line of spaces is all trailing");
    expect(spans_of("\t\tx", shaped(ListSpace::trail, false, true)).empty(),
           "listtab off marks no Tab");
    // 「あ　い　」: 全角空白は 1 字の桁（code point の序数）。
    expect(spans_of("\xE3\x81\x82\xE3\x80\x80\xE3\x81\x84\xE3\x80\x80", trail) ==
               std::vector<InvisibleSpan>{span(2, 3, InvisibleKind::ideographic_space),
                                          span(4, 5, InvisibleKind::ideographic_space)},
           "ideographic spaces are marked by their code point columns");
    expect(spans_of("\xE3\x80\x80\xE3\x80\x80x", trail) ==
               std::vector<InvisibleSpan>{span(1, 3, InvisibleKind::ideographic_space)},
           "neighbouring ideographic spaces share one span");
    expect(spans_of("\xE3\x80\x80x", shaped(ListSpace::trail, true, false)).empty(),
           "listwidespace off marks no ideographic space");
    expect(spans_of("", trail).empty() && spans_of("abc", trail).empty(),
           "a line without invisible characters has no span");
}

[[nodiscard]] core::TextBuffer buffer_of(std::string_view text)
{
    return core::TextBuffer::from_utf8(text).value();
}

void verify_line_breaks()
{
    const auto lf = buffer_of("a\nb\n\nc");
    expect(core::line_break_mark(lf, core::LineNumber{1}) == LineBreakMark::lf &&
               core::line_break_mark(lf, core::LineNumber{3}) == LineBreakMark::lf &&
               !core::line_break_mark(lf, core::LineNumber{4}).has_value(),
           "every line break is lf and the last line without one has no mark");
    const auto crlf = buffer_of("a\r\nb\nc\r\n");
    expect(core::line_break_mark(crlf, core::LineNumber{1}) == LineBreakMark::crlf &&
               core::line_break_mark(crlf, core::LineNumber{2}) == LineBreakMark::lf &&
               core::line_break_mark(crlf, core::LineNumber{3}) == LineBreakMark::crlf &&
               !core::line_break_mark(crlf, core::LineNumber{4}).has_value(),
           "a mixed document marks each line by its own ending");
    const auto lone = buffer_of("a\rb\nc\n");
    expect(core::line_break_mark(lone, core::LineNumber{1}) == LineBreakMark::lf,
           "a lone CR is the line's content (shown as ^M) and adds no mark");
}

void verify_names_and_colors()
{
    expect(core::list_space_named("none") == ListSpace::none &&
               core::list_space_named("trail") == ListSpace::trail &&
               core::list_space_named("all") == ListSpace::all &&
               !core::list_space_named("Trail").has_value() &&
               !core::list_space_named("").has_value() &&
               core::list_space_name(ListSpace::all) == "all",
           "the space names read none, trail and all only");
    const auto &palette = core::theme_of(core::BuiltinTheme::ubuntu_aubergine).ui;
    const auto plain = core::mix(palette.muted, palette.background, core::invisible_faded_percent);
    const auto strong =
        core::mix(palette.accent, palette.background, core::invisible_strong_faded_percent);
    expect(core::invisible_mark_color(palette, InvisibleKind::space) == plain &&
               core::invisible_mark_color(palette, InvisibleKind::tab) == plain &&
               core::line_break_mark_color(palette) == plain &&
               core::invisible_mark_color(palette, InvisibleKind::trailing_space) == strong &&
               core::invisible_mark_color(palette, InvisibleKind::ideographic_space) == strong,
           "trailing and ideographic spaces take the strong accent and the rest the faded muted");
}

// ---------------------------------------------------------------- `:set` と設定の画面（決定 3・4）

[[nodiscard]] std::optional<core::EditorSettings> set_by(std::string_view text,
                                                         const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value())
    {
        return std::nullopt;
    }
    return result.value().settings;
}

[[nodiscard]] std::string answer_of(std::string_view text, const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value() || result.value().settings.has_value())
    {
        return {};
    }
    return std::string(result.value().message.text());
}

[[nodiscard]] bool refused(std::string_view text, const core::EditorSettings &settings)
{
    return !core::evaluate_ex(text, settings, core::Appearance::dark).has_value();
}

[[nodiscard]] bool listed(const core::EditorSettings &settings)
{
    return core::feature_enabled(settings, core::EditorFeature::list);
}

void verify_list_switch()
{
    const auto settings = core::default_editor_settings();
    const auto on = set_by("set list", settings).value_or(settings);
    expect(listed(on) && on.features == std::vector<std::string>{"list"},
           ":set list writes the name into features");
    expect(!listed(set_by("set nolist", on).value_or(on)), ":set nolist takes it out");
    expect(answer_of("set list?", settings) == "list=off" &&
               answer_of("set list?", on) == "list=on",
           ":set list? answers the switch");
    expect(listed(set_by("set list!", settings).value_or(settings)) &&
               !listed(set_by("set invlist", on).value_or(on)) &&
               !listed(set_by("set list&", on).value_or(on)),
           "the switch toggles with ! and inv and resets to off");
    expect(refused("set listchars=tab:>-", settings) && refused("set listchars", settings),
           "listchars is not an option Nib knows");
}

void verify_kind_options()
{
    const auto settings = core::default_editor_settings();
    const auto off = set_by("set nolisteol", settings).value_or(settings);
    expect(!off.list_eol && answer_of("set listeol?", off) == "listeol=off" &&
               set_by("set listeol", off).value_or(off).list_eol &&
               set_by("set listeol&", off).value_or(off).list_eol,
           ":set listeol switches the line break marks and resets to on");
    expect(!set_by("set nolisttab", settings).value_or(settings).list_tab &&
               !set_by("set invlisttab", settings).value_or(settings).list_tab &&
               !set_by("set nolistwidespace", settings).value_or(settings).list_wide_space &&
               answer_of("set listwidespace?", settings) == "listwidespace=on",
           ":set listtab and listwidespace switch their marks");
    const auto all = set_by("set listspace=all", settings).value_or(settings);
    expect(all.list_space == ListSpace::all &&
               set_by("set listspace=none", settings).value_or(settings).list_space ==
                   ListSpace::none &&
               answer_of("set listspace", settings) == "listspace=trail" &&
               answer_of("set listspace?", all) == "listspace=all",
           ":set listspace= sets and the bare name asks");
    expect(refused("set listspace=trailing", settings) && refused("set listspace=", settings),
           "an unknown space value is refused");
    expect(set_by("set listspace&", all).value_or(all).list_space == ListSpace::trail,
           "listspace& resets to trail");
    expect(!listed(off) && !listed(all), "the kind options do not turn the whole switch on");
}

void verify_list_candidates()
{
    const auto candidates = core::ex_command_candidates();
    const auto has = [&candidates](std::string_view name)
    { return std::ranges::find(candidates, name) != candidates.end(); };
    expect(has("set list") && has("set nolist") && has("set listeol") && has("set nolisteol") &&
               has("set nolisttab") && has("set nolistwidespace") && has("set listspace=all") &&
               has("set listspace=trail") && has("set listspace=none"),
           "Ctrl+P lists the switch from the feature table and the kind options");
}

void verify_list_rows()
{
    const auto settings = core::default_editor_settings();
    const auto stepped = [&settings](core::SettingsValue value, bool forward)
    { return core::settings_value_stepped(settings, value, forward, {}); };
    expect(!stepped(core::SettingsValue::list_eol, true).list_eol &&
               !stepped(core::SettingsValue::list_tab, false).list_tab &&
               !stepped(core::SettingsValue::list_wide_space, true).list_wide_space &&
               stepped(core::SettingsValue::list_space, true).list_space == ListSpace::all &&
               stepped(core::SettingsValue::list_space, false).list_space == ListSpace::none,
           "the settings rows step the kinds");
    expect(core::settings_value_text(settings, core::SettingsValue::list_space,
                                     core::DisplayLanguage::japanese) == "行末だけ" &&
               core::settings_value_text(settings, core::SettingsValue::list_eol,
                                         core::DisplayLanguage::english) == "Show" &&
               core::settings_choice_texts(settings, core::SettingsValue::list_space, {},
                                           core::DisplayLanguage::english) ==
                   std::vector<std::string>{"None", "Line end only", "All"} &&
               core::settings_choice_position(settings, core::SettingsValue::list_tab, {}) ==
                   std::optional<std::size_t>{0},
           "the settings rows show the kinds in the display language");
    expect(
        core::settings_choice_applied(settings, core::SettingsValue::list_space, 0, {})
                    .list_space == ListSpace::none &&
            !core::settings_choice_applied(settings, core::SettingsValue::list_eol, 1, {}).list_eol,
        "choosing from the list sets the kind");
    const auto found = std::ranges::find_if(
        core::settings_rows, [](const core::SettingsRow &row)
        { return row.subject == core::SettingsSubject{core::EditorFeature::list}; });
    const std::array<core::SettingsValue, 4> kinds{
        core::SettingsValue::list_eol, core::SettingsValue::list_tab,
        core::SettingsValue::list_wide_space, core::SettingsValue::list_space};
    bool below = found != core::settings_rows.end() &&
                 found->section == core::SettingsSection::display &&
                 static_cast<std::size_t>(core::settings_rows.end() - found) > kinds.size();
    for (std::size_t index = 0; below && index < kinds.size(); ++index)
    {
        const auto &row = *(found + static_cast<std::ptrdiff_t>(index + 1));
        below = row.section == core::SettingsSection::display &&
                row.subject == core::SettingsSubject{kinds.at(index)};
    }
    expect(below, "the switch and its four kind rows sit in the display section in order");
    expect(core::feature_switchable(core::EditorFeature::list) &&
               !core::feature_cost(core::EditorFeature::list).default_on &&
               !core::feature_tier(core::EditorFeature::list).has_value() &&
               core::feature_name_of(core::EditorFeature::list) == "list",
           "the feature is listed, off by default and has no tier until measured");
}

// ---------------------------------------------------------------- frame の欄（決定 6・10・14・15）

constexpr std::string_view text_path = "C:\\work\\invisibles.txt";

[[nodiscard]] core::EditorSettings list_on()
{
    return core::feature_set(core::default_editor_settings(), core::EditorFeature::list, true);
}

void open_text(Editing &editing, std::string_view path, std::string text)
{
    editing.files().hold_at(std::string(path), Bytes{std::move(text)});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(path).value()});
}

[[nodiscard]] std::vector<std::optional<LineBreakMark>>
breaks_of(const std::vector<app::LineView> &lines)
{
    std::vector<std::optional<LineBreakMark>> breaks;
    for (const auto &line : lines)
    {
        breaks.push_back(line.invisibles.has_value() ? line.invisibles.value().line_break
                                                     : std::nullopt);
    }
    return breaks;
}

[[nodiscard]] std::vector<InvisibleSpan> spans_at(const std::vector<app::LineView> &lines,
                                                  std::size_t index)
{
    const std::vector<InvisibleSpan> missing{span(99, 99, InvisibleKind::space)};
    if (index >= lines.size())
    {
        return missing;
    }
    const auto &shown = lines[index].invisibles;
    return shown.has_value() ? shown.value().spans : missing;
}

void verify_frame_off_and_on()
{
    Editing off;
    applied(off.controller(), app::VisibleLines{10});
    open_text(off, text_path, "a  \n\tb\r\n");
    expect(std::ranges::none_of(off.controller().frame().lines, [](const app::LineView &line)
                                { return line.invisibles.has_value(); }),
           "with the feature off the frame has no invisible fields");
    Editing on(std::optional<core::EditorSettings>{list_on()});
    applied(on.controller(), app::VisibleLines{10});
    // CRLF の文書の LF の行（混在）。LF の文書の `\r\n` は `\r` が行の内容なので
    // lf（verify_line_breaks）。
    open_text(on, text_path, "a  \r\n\tb\nc");
    const auto frame = on.controller().frame();
    expect(frame.lines.size() == 3 &&
               spans_at(frame.lines, 0) ==
                   std::vector<InvisibleSpan>{span(2, 4, InvisibleKind::trailing_space)} &&
               spans_at(frame.lines, 1) ==
                   std::vector<InvisibleSpan>{span(1, 2, InvisibleKind::tab)} &&
               spans_at(frame.lines, 2).empty(),
           "the visible lines carry their spans");
    expect(breaks_of(frame.lines) == std::vector<std::optional<LineBreakMark>>{LineBreakMark::crlf,
                                                                               LineBreakMark::lf,
                                                                               std::nullopt},
           "each line carries its own line break and the last line without one has none");
    auto quiet = list_on();
    quiet.list_eol = false;
    quiet.list_tab = false;
    quiet.list_space = ListSpace::none;
    Editing kinds(std::optional<core::EditorSettings>{quiet});
    applied(kinds.controller(), app::VisibleLines{10});
    open_text(kinds, text_path, "a  \n\tb\r\nc");
    const auto bare = kinds.controller().frame();
    expect(bare.lines.size() == 3 &&
               std::ranges::all_of(bare.lines,
                                   [](const app::LineView &line)
                                   {
                                       return line.invisibles.has_value() &&
                                              line.invisibles.value().spans.empty() &&
                                              !line.invisibles.value().line_break.has_value();
                                   }),
           "every kind off draws nothing and is not refused");
}

void verify_frame_vim_and_split()
{
    Editing vim(std::optional<core::EditorSettings>{list_on()});
    applied(vim.controller(), app::VisibleLines{10});
    open_text(vim, text_path, "a\nb\n");
    expect(breaks_of(vim.controller().frame().lines) ==
               std::vector<std::optional<LineBreakMark>>{LineBreakMark::lf, LineBreakMark::lf,
                                                         std::nullopt},
           "the ordinary mode marks the final break and shows the empty line after it");
    applied(vim.controller(), app::SelectEditMode{core::EditMode::vim});
    const auto hidden = vim.controller().frame();
    expect(hidden.hidden_line_break &&
               breaks_of(hidden.lines) ==
                   std::vector<std::optional<LineBreakMark>>{LineBreakMark::lf, std::nullopt},
           "the Vim mode hides the final break and its mark");
    Editing split(std::optional<core::EditorSettings>{list_on()});
    applied(split.controller(), app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    open_text(split, text_path, "a \nb\n");
    const auto both = split.controller().apply_frame(app::SplitPane{core::SplitLayout::vertical});
    const auto *other =
        both.other.has_value() ? std::get_if<app::PaneFrame>(&both.other.value()) : nullptr;
    expect(other != nullptr && breaks_of(other->lines) == breaks_of(both.lines) &&
               spans_at(other->lines, 0) == spans_at(both.lines, 0) &&
               spans_at(both.lines, 0).size() == 1,
           "both panes carry the invisible marks");
}

void verify_frame_skipped_rows()
{
    Editing giant(std::optional<core::EditorSettings>{list_on()});
    applied(giant.controller(), app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    open_text(giant, text_path,
              "a \n" + std::string(core::giant_line_threshold + 1, 'x') + " \nb\n");
    applied(giant.controller(), app::SelectDisplay{core::DisplayForm::text});
    const auto windowed = giant.controller().frame();
    expect(windowed.lines.size() >= 3 && windowed.lines.at(1).window.active &&
               !windowed.lines.at(1).invisibles.has_value() &&
               windowed.lines.at(0).invisibles.has_value(),
           "a giant line's window has no marks while its neighbours keep theirs");
    Editing composing(std::optional<core::EditorSettings>{list_on()});
    applied(composing.controller(), app::VisibleLines{10});
    open_text(composing, text_path, "a \nb \n");
    applied(composing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{2}, Column{1}},
                            core::SelectionAnchoring::collapse});
    const auto composed =
        composing.controller().apply_frame(app::ComposeText{composed_of("\xE3\x81\x82", {}, 3)});
    expect(composed.lines.at(1).composed.has_value() &&
               !composed.lines.at(1).invisibles.has_value() &&
               composed.lines.at(0).invisibles.has_value(),
           "the IME composed line has no marks");
}
} // namespace

void verify_invisibles_scope()
{
    verify_spans();
    verify_line_breaks();
    verify_names_and_colors();
    verify_list_switch();
    verify_kind_options();
    verify_list_candidates();
    verify_list_rows();
    verify_frame_off_and_on();
    verify_frame_vim_and_split();
    verify_frame_skipped_rows();
}
} // namespace nenenib::tests
