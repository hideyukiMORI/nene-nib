// Issue #303 / ADR 0078。操作の割り当ての表・文字の表・鍵の表示名だけを測る。時計・ファイル・乱数を
// 使わない。
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "OperationModes.hpp"
#include "OperationTexts.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::core::EditMode;
using nenenib::core::EditorOperation;
using nenenib::core::KeyChord;
using nenenib::core::OperationKey;
using Operation = std::optional<EditorOperation>;

constexpr KeyChord ctrl(OperationKey key)
{
    return KeyChord{true, false, key};
}

constexpr KeyChord ctrl_shift(OperationKey key)
{
    return KeyChord{true, true, key};
}

// 鍵 → 通常モードの操作・Vim の操作。表の全行の鍵と、表に無い鍵の組み合わせ。
using ChordRow = std::tuple<KeyChord, Operation, Operation>;
constexpr std::array<ChordRow, 22> chord_rows{{
    {ctrl(OperationKey::o), EditorOperation::open_file, EditorOperation::open_file},
    {ctrl(OperationKey::s), EditorOperation::save, EditorOperation::save},
    {ctrl_shift(OperationKey::s), EditorOperation::save_as, EditorOperation::save_as},
    {ctrl(OperationKey::t), EditorOperation::new_tab, EditorOperation::new_tab},
    {ctrl(OperationKey::w), EditorOperation::close_tab, std::nullopt},
    {ctrl(OperationKey::f4), EditorOperation::close_tab, EditorOperation::close_tab},
    {ctrl(OperationKey::tab), EditorOperation::recent_tab, EditorOperation::recent_tab},
    {ctrl_shift(OperationKey::tab), EditorOperation::recent_tab_back,
     EditorOperation::recent_tab_back},
    {ctrl(OperationKey::p), EditorOperation::list_files, EditorOperation::list_files},
    {KeyChord{false, false, OperationKey::f1}, EditorOperation::list_operations,
     EditorOperation::list_operations},
    {ctrl(OperationKey::d), EditorOperation::toggle_bookmark, std::nullopt},
    {ctrl_shift(OperationKey::d), std::nullopt, EditorOperation::toggle_bookmark},
    {ctrl(OperationKey::z), EditorOperation::undo, std::nullopt},
    {ctrl(OperationKey::y), EditorOperation::redo, std::nullopt},
    {ctrl(OperationKey::plus), EditorOperation::font_larger, EditorOperation::font_larger},
    {ctrl(OperationKey::minus), EditorOperation::font_smaller, EditorOperation::font_smaller},
    {ctrl(OperationKey::zero), EditorOperation::font_reset, EditorOperation::font_reset},
    {ctrl_shift(OperationKey::o), std::nullopt, std::nullopt},
    {KeyChord{false, false, OperationKey::o}, std::nullopt, std::nullopt},
    {ctrl(OperationKey::f1), std::nullopt, std::nullopt},
    {ctrl_shift(OperationKey::w), std::nullopt, std::nullopt},
    {ctrl_shift(OperationKey::t), std::nullopt, std::nullopt},
}};

// 操作 → 通常モードで見せる鍵・Vim で見せる鍵（空は鍵なし）・通常で使えるか・Vim で使えるか。
// EditorOperation の順。
using ShownRow = std::tuple<EditorOperation, std::string_view, std::string_view, bool, bool>;
constexpr std::array<ShownRow, 16> shown_rows{{
    {EditorOperation::open_file, "Ctrl+O", "Ctrl+O", true, true},
    {EditorOperation::save, "Ctrl+S", "Ctrl+S", true, true},
    {EditorOperation::save_as, "Ctrl+Shift+S", "Ctrl+Shift+S", true, true},
    {EditorOperation::new_tab, "Ctrl+T", "Ctrl+T", true, true},
    {EditorOperation::close_tab, "Ctrl+W", "Ctrl+F4", true, true},
    {EditorOperation::recent_tab, "Ctrl+Tab", "Ctrl+Tab", true, true},
    {EditorOperation::recent_tab_back, "Ctrl+Shift+Tab", "Ctrl+Shift+Tab", true, true},
    {EditorOperation::list_files, "Ctrl+P", "Ctrl+P", true, true},
    {EditorOperation::list_operations, "F1", "F1", true, true},
    {EditorOperation::toggle_bookmark, "Ctrl+D", "Ctrl+Shift+D", true, true},
    {EditorOperation::undo, "Ctrl+Z", "", true, false},
    {EditorOperation::redo, "Ctrl+Y", "", true, false},
    {EditorOperation::font_larger, "Ctrl++", "Ctrl++", true, true},
    {EditorOperation::font_smaller, "Ctrl+-", "Ctrl+-", true, true},
    {EditorOperation::font_reset, "Ctrl+0", "Ctrl+0", true, true},
    {EditorOperation::toggle_mode, "", "", true, true},
}};

// 鍵の表示名。全 OperationKey と、Ctrl → Shift → 鍵の並び。
using LabelRow = std::pair<KeyChord, std::string_view>;
constexpr std::array<LabelRow, 17> label_rows{{
    {ctrl(OperationKey::o), "Ctrl+O"},
    {ctrl(OperationKey::s), "Ctrl+S"},
    {ctrl(OperationKey::t), "Ctrl+T"},
    {ctrl(OperationKey::w), "Ctrl+W"},
    {ctrl(OperationKey::p), "Ctrl+P"},
    {ctrl(OperationKey::d), "Ctrl+D"},
    {ctrl(OperationKey::z), "Ctrl+Z"},
    {ctrl(OperationKey::y), "Ctrl+Y"},
    {KeyChord{false, false, OperationKey::f1}, "F1"},
    {ctrl(OperationKey::f4), "Ctrl+F4"},
    {ctrl(OperationKey::tab), "Ctrl+Tab"},
    {ctrl(OperationKey::plus), "Ctrl++"},
    {ctrl(OperationKey::minus), "Ctrl+-"},
    {ctrl(OperationKey::zero), "Ctrl+0"},
    {ctrl_shift(OperationKey::s), "Ctrl+Shift+S"},
    {ctrl_shift(OperationKey::tab), "Ctrl+Shift+Tab"},
    {KeyChord{false, true, OperationKey::f1}, "Shift+F1"},
}};

std::string shown_label(EditorOperation operation, EditMode mode)
{
    const auto chord = nenenib::core::shown_chord(operation, mode);
    return chord.has_value() ? nenenib::core::key_chord_label(chord.value()) : std::string{};
}

void verify_operation_for()
{
    for (const auto &[chord, ordinary, vim] : chord_rows)
    {
        expect(nenenib::core::operation_for(chord, EditMode::ordinary) == ordinary,
               "a key in the ordinary mode maps to its operation from the table");
        expect(nenenib::core::operation_for(chord, EditMode::vim) == vim,
               "a key in the vim mode maps to its operation from the table");
    }
}

void verify_shown_and_available()
{
    expect(shown_rows.size() == nenenib::core::operation_texts.size(),
           "the expectations cover every operation");
    for (const auto &[operation, ordinary, vim, in_ordinary, in_vim] : shown_rows)
    {
        expect(shown_label(operation, EditMode::ordinary) == ordinary,
               "the ordinary mode shows the first key of the operation");
        expect(shown_label(operation, EditMode::vim) == vim,
               "the vim mode shows the first key of the operation");
        expect(nenenib::core::operation_available(operation, EditMode::ordinary) == in_ordinary,
               "an operation is available in the ordinary mode when a row covers it");
        expect(nenenib::core::operation_available(operation, EditMode::vim) == in_vim,
               "an operation is available in the vim mode when a row covers it");
    }
}

void verify_labels()
{
    for (const auto &[chord, label] : label_rows)
    {
        expect(nenenib::core::key_chord_label(chord) == label,
               "a key chord is labelled Ctrl, Shift and then the key");
    }
}

[[nodiscard]] bool rows_collide(const nenenib::core::OperationBinding &left,
                                const nenenib::core::OperationBinding &right)
{
    const bool shared_mode = (nenenib::core::modes_cover(left.modes, EditMode::ordinary) &&
                              nenenib::core::modes_cover(right.modes, EditMode::ordinary)) ||
                             (nenenib::core::modes_cover(left.modes, EditMode::vim) &&
                              nenenib::core::modes_cover(right.modes, EditMode::vim));
    return shared_mode && left.chord.has_value() && left.chord == right.chord;
}

void verify_no_shared_key()
{
    const auto &rows = nenenib::core::operation_bindings;
    for (std::size_t first = 0; first < rows.size(); ++first)
    {
        for (std::size_t second = first + 1; second < rows.size(); ++second)
        {
            expect(!rows_collide(rows.at(first), rows.at(second)),
                   "no two rows give one key in one mode to two operations");
        }
    }
}

void verify_texts()
{
    const auto &texts = nenenib::core::operation_texts;
    for (std::size_t index = 0; index < texts.size(); ++index)
    {
        const auto operation = static_cast<EditorOperation>(index);
        expect(std::ranges::count(texts, operation, &nenenib::core::OperationText::operation) == 1,
               "the text table holds every operation exactly once");
        const auto &text = nenenib::core::operation_text(operation);
        expect(text.operation == operation, "operation_text returns the row of the operation");
        expect(!text.name.empty() && !text.reading.empty() && !text.description.empty(),
               "every operation has a name, a reading and a description");
        expect(std::ranges::any_of(nenenib::core::operation_bindings,
                                   [&](const nenenib::core::OperationBinding &row)
                                   { return row.operation == operation; }),
               "every operation has a row in the binding table");
    }
    expect(nenenib::core::operation_text(EditorOperation::save).reading == "ほぞん",
           "the reading of save is hiragana");
}
} // namespace

void verify_operations_scope()
{
    verify_operation_for();
    verify_shown_and_available();
    verify_labels();
    verify_no_shared_key();
    verify_texts();
}
} // namespace nenenib::tests
