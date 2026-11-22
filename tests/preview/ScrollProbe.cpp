// 本文のスクロールの 1 刻みの費用の probe（#1905・ADR 0082 の閉じた比較の範囲・QLT-014 の正式な
// ゲートには入れない）。CTest に登録しない。窓を出さず、製品の BodyTextLayouts（DirectWrite）を
// controller の TextLayoutPort に結ぶ（ScrollProbeEditing）。1 刻み = apply(意図)・begin_frame と
// frame()（製品の draw_frame と同じ順）・frame の各行の get と glyphs_of（renderer の本文の文字組み
// の写し）と end_frame。D2D の描画呼出しと Present は含まない。時刻は Win32TimingAdapter の節目
// だけで取る（ARC-007）: 3 区間をそれぞれ probe_started / probe_finished の 1 対で挟み、節目の
// JSON（<json>.timing.json）を読み戻して中央値と最大を書く。暖機の 5 刻みは測らない。
//
//   nib_scroll_probe <道> <刻みの数 1..600> <json の出力先>
//
// 道（窓 1400×1000 px・96 DPI）:
//   wrap-wheel3         10,000 行の .cpp・折り返し・ScrollLines{3}
//   nowrap-long-wheel3  15,000 字の行 × 300・nowrap・ScrollLines{3}
//   page-down           10,000 行の .cpp・折り返し・通常モードの PgDn
//   page-wheel          10,000 行の .cpp・折り返し・WHEEL_PAGESCROLL（見えている行数）
//   vim-page            10,000 行の .cpp・折り返し・Vim のモードの <C-f>
//   grid-ascii-wheel3   16.8 MB の ASCII の 1 行・格子・ScrollLines{3}
//   grid-mixed-wheel3   16 MB の日本語と ASCII の混じった 1 行・格子・ScrollLines{3}
//   split-type          10,000 行の .cpp・折り返し・左右の 2 枠（同じ文書・等分）・InsertText{x}
//                       （draw は両枠の行・枠ごとの幅で問う・#1941）
#include "ScrollProbeEditing.hpp"

#include "BodyLayout.hpp"
#include "CommandText.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "Milestone.hpp"
#include "MoveCaret.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PaneLayouts.hpp"
#include "ScrollLines.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SubmitCommand.hpp"
#include "TextLayoutRequest.hpp"
#include "TextViewGeometry.hpp"
#include "Utf16.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimSpecialKey.hpp"
#include "VisibleLines.hpp"
#include "Win32TimingAdapter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <memory>
#include <objbase.h>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace application = nenenib::application;
using nenenib::tests::preview::ScrollProbeEditing;

constexpr std::int32_t window_width = 1400;
constexpr std::int32_t window_height = 1000;
constexpr std::uint32_t window_dpi = 96;
constexpr std::size_t warmup_steps = 5;
constexpr std::size_t marks_per_step = 6;
constexpr std::size_t most_steps = 600;
constexpr std::int64_t microseconds_per_millisecond = 1000;
constexpr std::size_t code_lines = 10'000;
constexpr std::size_t long_lines = 300;
constexpr std::size_t long_width = 15'000;
constexpr std::size_t giant_ascii_bytes = 16'800'000;
constexpr std::size_t giant_mixed_bytes = 16'000'000;
constexpr std::size_t giant_ascii_space_every = 7;

enum class ScrollPath : std::uint8_t
{
    wrap_wheel3,
    nowrap_long_wheel3,
    page_down,
    page_wheel,
    vim_page,
    grid_ascii_wheel3,
    grid_mixed_wheel3,
    split_type
};

constexpr std::array<std::string_view, 8> path_names{
    "wrap-wheel3", "nowrap-long-wheel3", "page-down",         "page-wheel",
    "vim-page",    "grid-ascii-wheel3",  "grid-mixed-wheel3", "split-type"};

// 名前の表の i 番目は ScrollPath の i 番目（path_of
// が添字で引く）。並びを崩したらコンパイルで落とす。
[[nodiscard]] constexpr std::string_view name_of(ScrollPath path)
{
    switch (path)
    {
    case ScrollPath::wrap_wheel3:
        return "wrap-wheel3";
    case ScrollPath::nowrap_long_wheel3:
        return "nowrap-long-wheel3";
    case ScrollPath::page_down:
        return "page-down";
    case ScrollPath::page_wheel:
        return "page-wheel";
    case ScrollPath::vim_page:
        return "vim-page";
    case ScrollPath::grid_ascii_wheel3:
        return "grid-ascii-wheel3";
    case ScrollPath::grid_mixed_wheel3:
        return "grid-mixed-wheel3";
    case ScrollPath::split_type:
        return "split-type";
    }
    std::unreachable();
}

[[nodiscard]] constexpr bool names_follow_paths()
{
    for (std::size_t index = 0; index < path_names.size(); ++index)
    {
        if (path_names.at(index) != name_of(static_cast<ScrollPath>(index)))
        {
            return false;
        }
    }
    return static_cast<std::size_t>(ScrollPath::split_type) + 1 == path_names.size();
}
static_assert(names_follow_paths(), "path_names must follow the order of ScrollPath");

[[nodiscard]] std::optional<ScrollPath> path_of(std::string_view name)
{
    for (std::size_t index = 0; index < path_names.size(); ++index)
    {
        if (path_names.at(index) == name)
        {
            return static_cast<ScrollPath>(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> steps_of(std::string_view text)
{
    std::size_t value = 0;
    const auto *end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end && value > 0 && value <= most_steps
               ? std::optional<std::size_t>{value}
               : std::nullopt;
}

// 調べの席 A の probe と同じ 5 つの形の行を繰り返すコード（行頭に通し番号の注釈）。
[[nodiscard]] std::string code_text()
{
    constexpr std::array<std::string_view, 5> shapes{
        "    // comment line that explains what the following function does in detail\r\n",
        "    static_cast<void>(controller.apply(application::ScrollLines{lines}));\r\n",
        "    for (std::size_t index = 0; index < frame.lines.size(); ++index) {\r\n",
        "        const auto value = compute(index, \"string literal 12345\", 0x7fU);\r\n",
        "    }\r\n"};
    std::string text;
    for (std::size_t line = 0; line < code_lines; ++line)
    {
        text += std::format("/*{}*/{}", line, shapes.at(line % shapes.size()));
    }
    return text;
}

[[nodiscard]] std::string long_text()
{
    std::string line;
    while (line.size() < long_width)
    {
        line += "lorem ipsum dolor sit amet consectetur adipiscing elit ";
    }
    line.resize(long_width);
    std::string text;
    for (std::size_t index = 0; index < long_lines; ++index)
    {
        text += std::format("{}{}\r\n", index, line);
    }
    return text;
}

[[nodiscard]] std::string giant_ascii_text()
{
    std::string text(giant_ascii_bytes, 'x');
    for (std::size_t at = 0; at < text.size(); at += giant_ascii_space_every)
    {
        text.at(at) = ' ';
    }
    return text;
}

// 「吾輩は猫である。名前はまだ無い。abc, def. 」の繰り返し（UTF-8 の固定列）。
[[nodiscard]] std::string giant_mixed_text()
{
    const std::string unit = "\xE5\x90\xBE\xE8\xBC\xA9\xE3\x81\xAF\xE7\x8C\xAB\xE3\x81\xA7\xE3"
                             "\x81\x82\xE3\x82\x8B\xE3\x80\x82\xE5\x90\x8D\xE5\x89\x8D\xE3\x81"
                             "\xAF\xE3\x81\xBE\xE3\x81\xA0\xE7\x84\xA1\xE3\x81\x84\xE3\x80\x82"
                             "abc, def. ";
    std::string text;
    text.reserve(giant_mixed_bytes + unit.size());
    while (text.size() < giant_mixed_bytes)
    {
        text += unit;
    }
    return text;
}

[[nodiscard]] std::string text_of(ScrollPath path)
{
    switch (path)
    {
    case ScrollPath::wrap_wheel3:
    case ScrollPath::page_down:
    case ScrollPath::page_wheel:
    case ScrollPath::vim_page:
    case ScrollPath::split_type:
        return code_text();
    case ScrollPath::nowrap_long_wheel3:
        return long_text();
    case ScrollPath::grid_ascii_wheel3:
        return giant_ascii_text();
    case ScrollPath::grid_mixed_wheel3:
        return giant_mixed_text();
    }
    std::unreachable();
}

[[nodiscard]] std::string_view file_of(ScrollPath path)
{
    switch (path)
    {
    case ScrollPath::wrap_wheel3:
    case ScrollPath::page_down:
    case ScrollPath::page_wheel:
    case ScrollPath::vim_page:
    case ScrollPath::split_type:
        return "C:\\nib-probe\\code.cpp";
    case ScrollPath::nowrap_long_wheel3:
        return "C:\\nib-probe\\long.txt";
    case ScrollPath::grid_ascii_wheel3:
        return "C:\\nib-probe\\giant.json";
    case ScrollPath::grid_mixed_wheel3:
        return "C:\\nib-probe\\giant.txt";
    }
    std::unreachable();
}

// 本文の枠（製品の body_panes と同じ pane_layouts の 1 本・1 枠なら body_layout と同じ値）。
[[nodiscard]] core::PaneLayouts panes_of(const application::EditorController &controller)
{
    return core::pane_layouts(core::LayoutRect{0, 0, window_width, window_height}, window_dpi,
                              controller.delivery().settings.font_size, controller.split().layout);
}

[[nodiscard]] core::BodyLayout body_of(const application::EditorController &controller)
{
    return core::active_pane_layout(panes_of(controller), controller.split().active_side);
}

// WHEEL_PAGESCROLL は見えている行数ぶんの ScrollLines（製品の scroll_body と同じ量）。
[[nodiscard]] application::EditorIntent intent_of(ScrollPath path,
                                                  const application::EditorController &controller)
{
    switch (path)
    {
    case ScrollPath::page_down:
        return application::MoveCaret{core::CaretMotion::page_down,
                                      core::SelectionAnchoring::collapse};
    case ScrollPath::page_wheel:
        return application::ScrollLines{
            static_cast<std::int32_t>(body_of(controller).visible_lines)};
    case ScrollPath::vim_page:
        return application::VimKeyPress{core::VimKey{core::VimSpecialKey::control_f}};
    case ScrollPath::split_type:
        return application::InsertText{"x"};
    case ScrollPath::wrap_wheel3:
    case ScrollPath::nowrap_long_wheel3:
    case ScrollPath::grid_ascii_wheel3:
    case ScrollPath::grid_mixed_wheel3:
        return application::ScrollLines{3};
    }
    std::unreachable();
}

// 本文の幅・行高・DPI と、本文の書式の字幅（製品の body_view と同じ値）。
[[nodiscard]] core::TextViewGeometry geometry_of(ScrollProbeEditing &editing,
                                                 const core::BodyLayout &body)
{
    const auto settings = editing.controller().delivery().settings;
    core::TextViewGeometry geometry{std::max(core::width_of(body.content), 1), body.line_height,
                                    window_dpi};
    const auto advance = editing.layouts().cell(
        application::TextLayoutRequest{{}, settings, geometry, core::DisplayForm::text});
    geometry.average_advance = advance.has_value() ? advance.value().width : 0.0F;
    return geometry;
}

[[nodiscard]] core::TextViewGeometry geometry_of(ScrollProbeEditing &editing)
{
    return geometry_of(editing, body_of(editing.controller()));
}

void send_view(ScrollProbeEditing &editing)
{
    const auto body = body_of(editing.controller());
    static_cast<void>(editing.controller().apply(
        application::VisibleLines{body.visible_lines, geometry_of(editing), window_width,
                                  window_height, body.visible_lines}));
}

// 枠の寸法と横の送り（引数の数を tidy の上限に収める）。
using PaneDraw = std::pair<core::TextViewGeometry, float>;

void draw_text_lines(ScrollProbeEditing &editing, const application::EditorFrame &frame,
                     std::span<const application::LineView> lines, PaneDraw pane)
{
    const auto &[geometry, horizontal] = pane;
    auto &layouts = editing.layouts();
    constexpr auto bottom = static_cast<float>(window_height);
    for (const auto &line : lines)
    {
        const auto made = layouts.get(application::TextLayoutRequest{
            line.display.text, frame.settings, geometry, core::DisplayForm::text, {}, line.window});
        if (made.has_value())
        {
            static_cast<void>(layouts.glyphs_of(made.value().Get(), -horizontal, 0.0F, bottom));
        }
    }
}

// renderer の本文の文字組み（行ごとの get → glyphs_of・格子の画面行も同じ）の写し。もう 1 つの
// 本文の枠は renderer と同じく、その枠の配置の幅で問う。
void draw_lines(ScrollProbeEditing &editing, const application::EditorFrame &frame,
                core::TextViewGeometry geometry)
{
    auto &layouts = editing.layouts();
    constexpr auto bottom = static_cast<float>(window_height);
    draw_text_lines(editing, frame, frame.lines, PaneDraw{geometry, frame.horizontal});
    const auto *const other =
        frame.other.has_value() ? std::get_if<application::PaneFrame>(&frame.other.value())
                                : nullptr;
    const auto other_body = core::other_pane_layout(panes_of(editing.controller()),
                                                    editing.controller().split().active_side);
    if (other != nullptr && other_body.has_value())
    {
        draw_text_lines(editing, frame, other->lines,
                        PaneDraw{geometry_of(editing, other_body.value()), other->horizontal});
    }
    for (const auto &row : frame.grid)
    {
        const auto made = layouts.get(application::TextLayoutRequest{
            row.text, frame.settings, geometry, core::DisplayForm::grid, row.starts});
        if (made.has_value())
        {
            static_cast<void>(layouts.glyphs_of(made.value().Get(), 0.0F, 0.0F, bottom));
        }
    }
}

// 1 刻み。製品の draw_frame と同じく begin_frame の後に frame() を作る。
void step(ScrollProbeEditing &editing, const application::EditorIntent &intent,
          application::TimingPort &timing)
{
    auto &controller = editing.controller();
    const auto geometry = geometry_of(editing);
    timing.mark(core::Milestone::probe_started);
    static_cast<void>(controller.apply(intent));
    timing.mark(core::Milestone::probe_finished);
    timing.mark(core::Milestone::probe_started);
    editing.layouts().begin_frame();
    const auto frame = controller.frame();
    timing.mark(core::Milestone::probe_finished);
    timing.mark(core::Milestone::probe_started);
    draw_lines(editing, frame, geometry);
    editing.layouts().end_frame();
    timing.mark(core::Milestone::probe_finished);
}

void open(ScrollProbeEditing &editing, ScrollPath path, std::string text)
{
    auto &controller = editing.controller();
    editing.files().hold(std::move(text));
    send_view(editing);
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse(std::string(file_of(path))).value()}));
    if (path == ScrollPath::nowrap_long_wheel3)
    {
        static_cast<void>(controller.apply(application::OpenCommandPalette{}));
        static_cast<void>(controller.apply(application::CommandText{":set nowrap"}));
        static_cast<void>(controller.apply(application::SubmitCommand{}));
    }
    static_cast<void>(controller.apply(application::SelectEditMode{
        path == ScrollPath::vim_page ? core::EditMode::vim : core::EditMode::ordinary}));
    if (path == ScrollPath::grid_ascii_wheel3 || path == ScrollPath::grid_mixed_wheel3)
    {
        static_cast<void>(controller.apply(application::SelectDisplay{core::DisplayForm::grid}));
    }
    if (path == ScrollPath::split_type)
    {
        static_cast<void>(controller.apply(application::SplitPane{core::SplitLayout::vertical}));
    }
    send_view(editing);
}

// 開いた後の設定が道の名前どおりか（設定の道が変わって nowrap や格子が黙って効かなくなったら、
// 別の道を測らずに失敗で終わる）。
[[nodiscard]] bool opened_as(application::EditorController &controller, ScrollPath path)
{
    const bool unwrapped = path == ScrollPath::nowrap_long_wheel3;
    const bool gridded =
        path == ScrollPath::grid_ascii_wheel3 || path == ScrollPath::grid_mixed_wheel3;
    const bool split = path == ScrollPath::split_type;
    const auto frame = controller.frame();
    return frame.other.has_value() == split &&
           (frame.settings.wrapping == core::LineWrapping::no_wrap) == unwrapped &&
           frame.grid.empty() != gridded && frame.lines.empty() == gridded;
}

// 道を開いて暖機の後に steps 刻みを測る。節目は timing のデストラクタが書く。
// 戻り値は最後の frame の先頭の論理行と画面行の数（同じ道の前後で一致するかの照合）。
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
timed_run(ScrollPath path, std::size_t steps, const std::wstring &timing_path)
{
    ScrollProbeEditing editing;
    open(editing, path, text_of(path));
    if (!opened_as(editing.controller(), path))
    {
        return std::nullopt;
    }
    const auto intent = intent_of(path, editing.controller());
    nenenib::adapters::win32::Win32TimingAdapter timing;
    for (std::size_t index = 0; index < warmup_steps; ++index)
    {
        step(editing, intent, timing);
    }
    timing.bind(timing_path);
    for (std::size_t index = 0; index < steps; ++index)
    {
        step(editing, intent, timing);
    }
    const auto last = editing.controller().frame();
    return std::pair{last.first_visible.value, last.lines.size() + last.grid.size()};
}

[[nodiscard]] std::optional<std::string> read_text(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// 節目の JSON の qpcMicroseconds を順に読む（nib_preview_probe と同じ読み）。
[[nodiscard]] std::vector<std::int64_t> mark_times(std::string_view report)
{
    constexpr std::string_view key = "\"qpcMicroseconds\": ";
    std::vector<std::int64_t> times;
    for (auto at = report.find(key); at != std::string_view::npos; at = report.find(key, at))
    {
        at += key.size();
        std::int64_t value = 0;
        const auto [stop, error] =
            std::from_chars(report.data() + at, report.data() + report.size(), value);
        if (error != std::errc{})
        {
            return {};
        }
        times.push_back(value);
        at = static_cast<std::size_t>(stop - report.data());
    }
    return times;
}

[[nodiscard]] std::string milliseconds_text(std::int64_t microseconds)
{
    return std::format("{}.{:03d}", microseconds / microseconds_per_millisecond,
                       microseconds % microseconds_per_millisecond);
}

// 刻みごとの区間 [from, to)（節目の添字）の長さの中央値と最大。
[[nodiscard]] std::string spread_of(std::span<const std::int64_t> times, std::size_t from,
                                    std::size_t to)
{
    std::vector<std::int64_t> spans;
    for (std::size_t at = 0; at + marks_per_step <= times.size(); at += marks_per_step)
    {
        spans.push_back(times[at + to] - times[at + from]);
    }
    std::ranges::sort(spans);
    return std::format("{{\"medianMs\":{},\"maxMs\":{}}}",
                       milliseconds_text(spans.at(spans.size() / 2)),
                       milliseconds_text(spans.back()));
}

[[nodiscard]] std::string report_of(std::string_view path, std::size_t steps,
                                    std::pair<std::size_t, std::size_t> last,
                                    std::span<const std::int64_t> times)
{
    return std::format(
        "{{\"schema\":1,\"probe\":\"nib_scroll_probe\",\"path\":\"{}\",\"steps\":{},"
        "\"warmup\":{},\"step\":{},\"apply\":{},\"frame\":{},\"draw\":{},\"firstVisible\":{},"
        "\"rows\":{},\"window\":{{\"widthPx\":{},\"heightPx\":{},\"dpi\":{}}},\"commit\":\"{}\","
        "\"dirty\":{},\"configuration\":\"{}\"}}\n",
        path, steps, warmup_steps, spread_of(times, 0, marks_per_step - 1), spread_of(times, 0, 1),
        spread_of(times, 2, 3), spread_of(times, 4, 5), last.first, last.second, window_width,
        window_height, window_dpi, NIB_PROBE_COMMIT, NIB_PROBE_DIRTY, NIB_PROBE_CONFIGURATION);
}

[[nodiscard]] int measured(ScrollPath path, std::size_t steps, const std::string &output)
{
    const std::string timing_path = output + ".timing.json";
    const auto wide = core::to_utf16(timing_path);
    if (!wide.has_value())
    {
        return 2;
    }
    const auto last = timed_run(path, steps, wide.value());
    if (!last.has_value())
    {
        std::fputs("scroll probe: the document did not open with the path's settings\n", stderr);
        return 1;
    }
    const auto timing = read_text(timing_path);
    const auto times =
        timing.has_value() ? mark_times(timing.value()) : std::vector<std::int64_t>{};
    if (times.size() != steps * marks_per_step)
    {
        std::fputs("scroll probe: the timing file has the wrong number of marks\n", stderr);
        return 1;
    }
    const auto text =
        report_of(path_names.at(static_cast<std::size_t>(path)), steps, last.value(), times);
    std::ofstream file(output, std::ios::binary);
    file << text;
    std::fputs(text.c_str(), stdout);
    return file ? 0 : 1;
}
} // namespace

int main(int argc, char **argv)
{
    const std::span<char *> arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 4)
    {
        std::fputs("usage: nib_scroll_probe <path> <steps 1..600> <json>\n", stderr);
        return 2;
    }
    const auto path = path_of(arguments[1]);
    const auto steps = steps_of(arguments[2]);
    if (!path.has_value() || !steps.has_value())
    {
        std::fputs("scroll probe: unknown path or steps out of 1..600\n", stderr);
        return 2;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return 1;
    }
    const int code = measured(path.value(), steps.value(), arguments[3]);
    CoUninitialize();
    return code;
}
