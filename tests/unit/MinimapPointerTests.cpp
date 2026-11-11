// scope `--minimap-pointer` の単体テスト（ADR 0171 の決定 5 の追補・Issue #1371 の段 2 の 1
// 本目）。 core の点 → 面の段（minimap_pointed_row）と段 →
// 見えている最初の行（minimap_pointed_first）の 端（1
// 行の文書・面より短い文書・面より長い文書・面が見えている行より短い・比の丸め）と、固定 seed
// の性質（文書の中・同じ段を 2 回押しても動かない・段について単調・押した段が見えている範囲の面の
// 中央に来る）、application の契約（意図の列で窓の先頭が期待の行・キャレットと選択と本文は不変・
// undo と `.` と Vim の録画は動かない・面が出ていなければ何もしない）。時計・ファイル・乱数の種を
// 読まない（std::mt19937_64 に固定 seed・剰余で範囲に畳む）。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "Minimap.hpp"
#include "MinimapLayout.hpp"
#include "MinimapPointing.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "PointMinimap.hpp"
#include "PreviewPlacement.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SetPreviewPlacement.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::LineNumber;
using Random = std::mt19937_64;

constexpr std::string_view code_path = "C:\\work\\pointer.cpp";
constexpr std::int32_t window_width = 1000;
constexpr std::int32_t window_height = 700;

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

// 点 → 面の段は面の上端から 2 物理画素ずつ。上へ出た点は切り下げで負の段。
void verify_pointed_row()
{
    expect(core::minimap_pointed_row(60, 60) == 0 && core::minimap_pointed_row(61, 60) == 0 &&
               core::minimap_pointed_row(62, 60) == 1 && core::minimap_pointed_row(661, 60) == 300,
           "a point in the panel is its row of two pixels");
    expect(core::minimap_pointed_row(59, 60) == -1 && core::minimap_pointed_row(58, 60) == -1 &&
               core::minimap_pointed_row(57, 60) == -2 && core::minimap_pointed_row(0, 60) == -30,
           "a point above the panel is a negative row rounded down");
}

// 段 → 見えている最初の行の端。見えている範囲の面の中央が押した段に来る。
void verify_pointed_first()
{
    expect(core::minimap_pointed_first(0, 20, 1, 300) == LineNumber{1} &&
               core::minimap_pointed_first(150, 20, 1, 300) == LineNumber{1} &&
               core::minimap_pointed_first(-5, 20, 20, 300) == LineNumber{1},
           "a document within one screen stays at line 1");
    // 面より短い文書（面は送られない）: 段 r は行 r + 1 で、それを見えている 20 行の中央へ。
    expect(core::minimap_pointed_first(50, 20, 100, 300) == LineNumber{41} &&
               core::minimap_pointed_first(5, 20, 100, 300) == LineNumber{1} &&
               core::minimap_pointed_first(299, 20, 100, 300) == LineNumber{81} &&
               core::minimap_pointed_first(-40, 20, 100, 300) == LineNumber{1},
           "a document shorter than the panel centres the pressed line");
    // 面より長い文書: 見えている範囲の面の上端は 0〜80 段を動き、文書の 0〜980 行に比例する。
    expect(core::minimap_pointed_first(10, 20, 1000, 100) == LineNumber{1} &&
               core::minimap_pointed_first(50, 20, 1000, 100) == LineNumber{491} &&
               core::minimap_pointed_first(90, 20, 1000, 100) == LineNumber{981} &&
               core::minimap_pointed_first(1000, 20, 1000, 100) == LineNumber{981},
           "a document longer than the panel follows the face in proportion");
    // 段 50 の 491 行は minimap_top の 451 行の面で 41 段目からの 20 段（中央が 50 段）。
    expect(core::minimap_top(LineNumber{491}, 20, 1000, 100) == LineNumber{451},
           "the pressed row is the middle of the face after the scroll");
    // 丸めは四捨五入: 981 行を 80 段に割ると 1 段 12.26 行で、3 段は 36.79 → 37。
    expect(core::minimap_pointed_first(11, 20, 1001, 100) == LineNumber{13} &&
               core::minimap_pointed_first(13, 20, 1001, 100) == LineNumber{38},
           "the ratio is rounded to the nearest line");
    // 面が見えている行より短い: 面の段の割合を文書に写す。
    expect(core::minimap_pointed_first(0, 20, 1000, 10) == LineNumber{1} &&
               core::minimap_pointed_first(9, 20, 1000, 10) == LineNumber{981} &&
               core::minimap_pointed_first(4, 20, 1000, 10) == LineNumber{437} &&
               core::minimap_pointed_first(3, 20, 1000, 0) == LineNumber{981},
           "a panel shorter than the screen maps its rows over the document");
}

// 固定 seed の性質。結果は文書の中・段について単調・今のスクロールに依らない（同じ段を 2 回押しても
// 同じ行）・面が送られる文書では送った後の見えている範囲の面の上端が押した段の中央の位置から 1 段
// 以内。
void verify_pointed_properties()
{
    Random random{1371};
    std::size_t checked = 0;
    for (std::size_t trial = 0; trial < 4'000; ++trial)
    {
        const std::size_t total = 1 + below(random, 20'000);
        const std::size_t visible = 1 + below(random, 80);
        const std::size_t rows = below(random, 400);
        const auto row_bound = static_cast<std::int64_t>(rows) + 100;
        const auto row = static_cast<std::int32_t>(
            static_cast<std::int64_t>(below(random, static_cast<std::size_t>(row_bound))) - 50);
        const auto next = row + static_cast<std::int32_t>(below(random, 30));
        const auto first = core::minimap_pointed_first(row, visible, total, rows);
        const std::size_t last = total > visible ? total - visible + 1 : 1;
        expect(first.value >= 1 && first.value <= last, "the pointed line is in the document");
        expect(core::minimap_pointed_first(next, visible, total, rows).value >= first.value,
               "dragging down never scrolls up");
        expect(core::minimap_pointed_first(row, visible, total, rows) == first,
               "pressing the same row twice gives the same line");
        if (total > rows && rows > visible)
        {
            const auto top = core::minimap_top(first, visible, total, rows);
            const auto face = static_cast<std::int64_t>(first.value - top.value);
            const auto track = static_cast<std::int64_t>(rows - visible);
            const std::int64_t wanted = std::clamp<std::int64_t>(
                std::int64_t{row} - static_cast<std::int64_t>(visible / 2), 0, track);
            expect(std::abs(face - wanted) <= 1,
                   "the face is drawn around the pressed row after the scroll");
            ++checked;
        }
    }
    expect(checked > 1'000, "most trials scroll the panel");
}

[[nodiscard]] core::EditorSettings minimap_settings()
{
    return core::minimap_set(core::default_editor_settings(), true);
}

[[nodiscard]] std::string numbered_lines(std::size_t count)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += "int value_" + std::to_string(line) + " = " + std::to_string(line) + ";\n";
    }
    return text;
}

void open_numbered(Editing &editing, std::size_t lines)
{
    editing.files().hold_at(std::string(code_path), Bytes{numbered_lines(lines)});
    applied(editing.controller(), app::VisibleLines{20, {}, window_width, window_height});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(code_path).value()});
}

// 窓の寸法から面の段の数（controller と同じ core::minimap_layout の 1 本）。
[[nodiscard]] std::size_t panel_rows()
{
    return core::minimap_layout(core::LayoutRect{0, 0, window_width, window_height},
                                core::TextViewGeometry{}.dpi,
                                core::default_editor_settings().font_size, true)
        .rows;
}

[[nodiscard]] app::EditorFrame pointed(app::EditorController &controller,
                                       app::MinimapPointing pointing, std::int32_t row)
{
    static_cast<void>(controller.apply(app::PointMinimap{pointing, row}));
    return controller.frame();
}

// 押した段・同じ段の 2 回目・ドラッグ。窓の先頭だけが動き、キャレットと本文は動かない。
void verify_press_and_drag()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_numbered(editing, 6000);
    // 本文は改行で終わるので、行の数は最後の空の行を含めて 6,001。
    const std::size_t lines = 6001;
    const auto before = controller.frame();
    const std::size_t visible = before.minimap.has_value() ? before.minimap.value().visible : 0;
    const std::size_t rows = panel_rows();
    expect(rows > visible && visible > 0, "the panel holds more rows than the screen");
    const auto pressed = pointed(controller, app::MinimapPointing::pressed, 150);
    expect(pressed.first_visible == core::minimap_pointed_first(150, visible, lines, rows) &&
               pressed.first_visible.value > 1,
           "pressing the panel scrolls to the pressed line");
    expect(pressed.caret == before.caret && pressed.lines.front().text != before.lines.front().text,
           "pressing the panel moves the window but not the caret");
    const auto again = pointed(controller, app::MinimapPointing::pressed, 150);
    expect(again.first_visible == pressed.first_visible, "pressing the same row again stays put");
    const auto lower = pointed(controller, app::MinimapPointing::dragged, 200);
    const auto lowest = pointed(controller, app::MinimapPointing::dragged, 10'000);
    const auto above = pointed(controller, app::MinimapPointing::dragged, -10);
    expect(pressed.first_visible.value < lower.first_visible.value &&
               lower.first_visible.value < lowest.first_visible.value &&
               above.first_visible == LineNumber{1},
           "dragging follows the pointer and stops at the ends");
    expect(lowest.first_visible == LineNumber{lines - visible + 1} && lowest.caret == before.caret,
           "dragging below the panel shows the last screen without moving the caret");
}

// 編集の後に押しても undo の単位は増えず、undo は編集だけを戻す。面が出ていなければ何もしない。
void verify_history_and_off()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_numbered(editing, 3000);
    applied(controller, app::InsertText{"x"});
    const auto typed = controller.frame();
    const auto scrolled = pointed(controller, app::MinimapPointing::pressed, 200);
    expect(scrolled.first_visible.value > 1 && scrolled.caret == typed.caret,
           "pressing after an edit scrolls without moving the caret");
    static_cast<void>(controller.apply(app::HistoryAction{core::HistoryDirection::undo}));
    static_cast<void>(controller.apply(app::PlaceCaret{
        core::TextPosition{LineNumber{1}, core::Column{1}}, core::SelectionAnchoring::collapse}));
    const auto undone = controller.frame();
    expect(undone.lines.front().text == "int value_1 = 1;",
           "undo takes back the edit and not the scroll");

    Editing off;
    open_numbered(off, 3000);
    const auto plain = off.controller().frame();
    const auto ignored = pointed(off.controller(), app::MinimapPointing::pressed, 200);
    expect(!plain.minimap.has_value() && ignored.first_visible == plain.first_visible,
           "without the minimap a press does nothing");

    Editing full{SettingsReading{std::optional{minimap_settings()}}};
    open_numbered(full, 3000);
    static_cast<void>(
        full.controller().apply(app::SetPreviewPlacement{app::PreviewPlacement::full}));
    const auto previewing = full.controller().frame();
    const auto refused = pointed(full.controller(), app::MinimapPointing::pressed, 200);
    expect(!refused.minimap.has_value() && refused.first_visible == previewing.first_visible,
           "the full preview has no minimap to press");
}

// Vim のモード: 押しても `.` の記録と録画は残り、キャレットは寄せない（ホイールと同じ）。
void verify_vim_mode()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_numbered(editing, 3000);
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    vim_replay(controller, "qax");
    const auto edited = controller.frame();
    const auto scrolled = pointed(controller, app::MinimapPointing::pressed, 200);
    expect(scrolled.first_visible.value > 1 && scrolled.caret == edited.caret &&
               controller.vim_state().macro_recording.has_value(),
           "pressing in Vim scrolls and keeps the caret and the recording");
    vim_replay(controller, "q.");
    static_cast<void>(controller.apply(app::PlaceCaret{
        core::TextPosition{LineNumber{1}, core::Column{1}}, core::SelectionAnchoring::collapse}));
    const auto repeated = controller.frame();
    expect(repeated.lines.front().text == "t value_1 = 1;",
           "dot repeats the change made before the press");
}
} // namespace

void verify_minimap_pointer_scope()
{
    verify_pointed_row();
    verify_pointed_first();
    verify_pointed_properties();
    verify_press_and_drag();
    verify_history_and_off();
    verify_vim_mode();
}
} // namespace nenenib::tests
