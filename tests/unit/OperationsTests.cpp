// Issue #303 / ADR 0078。操作の割り当ての表・文字の表・鍵の表示名だけを測る。時計・ファイル・乱数を
// 使わない。
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
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
#include <vector>

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
constexpr std::array<ChordRow, 46> chord_rows{{
    // Vim の Ctrl+O はジャンプの戻り（D71・#1037）。Vim の開くは Ctrl+Shift+O。
    {ctrl(OperationKey::o), EditorOperation::open_file, std::nullopt},
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
    {ctrl_shift(OperationKey::space), EditorOperation::toggle_mode, EditorOperation::toggle_mode},
    // Ctrl+Space（Shift なし）は Vim INSERT の Ctrl-@ に残すので割り当てない（D61）。
    {ctrl(OperationKey::space), std::nullopt, std::nullopt},
    // Shift を見ていなかった今の振る舞いを保つ行（Vim の Ctrl+Shift+Z / Y は値なし）。
    {ctrl_shift(OperationKey::o), EditorOperation::open_file, EditorOperation::open_file},
    {ctrl_shift(OperationKey::p), EditorOperation::list_files, EditorOperation::list_files},
    {ctrl_shift(OperationKey::z), EditorOperation::undo, std::nullopt},
    {ctrl_shift(OperationKey::y), EditorOperation::redo, std::nullopt},
    {ctrl_shift(OperationKey::plus), EditorOperation::font_larger, EditorOperation::font_larger},
    {ctrl_shift(OperationKey::minus), EditorOperation::font_smaller, EditorOperation::font_smaller},
    {ctrl_shift(OperationKey::zero), EditorOperation::font_reset, EditorOperation::font_reset},
    {KeyChord{false, false, OperationKey::o}, std::nullopt, std::nullopt},
    {ctrl(OperationKey::f1), std::nullopt, std::nullopt},
    {ctrl_shift(OperationKey::w), std::nullopt, std::nullopt},
    {ctrl_shift(OperationKey::t), std::nullopt, std::nullopt},
    // 枠の操作（ADR 0148 の決定 6）。Ctrl+Shift+F4 は枠を閉じ、Ctrl+F4 はタブを閉じたまま。
    {ctrl(OperationKey::backslash), EditorOperation::split_vertical,
     EditorOperation::split_vertical},
    {ctrl_shift(OperationKey::backslash), EditorOperation::split_horizontal,
     EditorOperation::split_horizontal},
    {KeyChord{false, false, OperationKey::f6}, EditorOperation::next_pane,
     EditorOperation::next_pane},
    {KeyChord{false, true, OperationKey::f6}, EditorOperation::next_pane,
     EditorOperation::next_pane},
    {ctrl_shift(OperationKey::f4), EditorOperation::close_pane, EditorOperation::close_pane},
    {ctrl(OperationKey::f6), std::nullopt, std::nullopt},
    {KeyChord{false, false, OperationKey::backslash}, std::nullopt, std::nullopt},
    // Markdown のプレビュー（ADR 0155 の決定 3）。Ctrl+M（Shift なし）は割り当てない。
    {ctrl_shift(OperationKey::m), EditorOperation::toggle_preview, EditorOperation::toggle_preview},
    {ctrl(OperationKey::m), std::nullopt, std::nullopt},
    // 文字だけの貼り付け（ADR 0157 の決定 2）。Ctrl+V（Shift なし）は表の外（ui の貼り付けの道）。
    {ctrl_shift(OperationKey::v), EditorOperation::paste_plain, EditorOperation::paste_plain},
    {ctrl(OperationKey::v), std::nullopt, std::nullopt},
    // エクスプローラー（ADR 0162 の決定 7）。Ctrl+E（Shift なし）は割り当てない。
    {ctrl_shift(OperationKey::e), EditorOperation::toggle_explorer,
     EditorOperation::toggle_explorer},
    {ctrl(OperationKey::e), std::nullopt, std::nullopt},
    // 日時の挿入（ADR 0175 の決定 3）。修飾なしの F5 だけ・両モード。
    {KeyChord{false, false, OperationKey::f5}, EditorOperation::insert_date_time,
     EditorOperation::insert_date_time},
    {KeyChord{false, true, OperationKey::f5}, std::nullopt, std::nullopt},
    {ctrl(OperationKey::f5), std::nullopt, std::nullopt},
}};

// 操作 → 通常モードで見せる鍵・Vim で見せる鍵（空は鍵なし）・通常で使えるか・Vim で使えるか。
// EditorOperation の順。
using ShownRow = std::tuple<EditorOperation, std::string_view, std::string_view, bool, bool>;
constexpr std::array<ShownRow, 28> shown_rows{{
    {EditorOperation::open_file, "Ctrl+O", "Ctrl+Shift+O", true, true},
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
    {EditorOperation::toggle_mode, "Ctrl+Shift+Space", "Ctrl+Shift+Space", true, true},
    {EditorOperation::split_vertical, "Ctrl+\\", "Ctrl+\\", true, true},
    {EditorOperation::split_horizontal, "Ctrl+Shift+\\", "Ctrl+Shift+\\", true, true},
    {EditorOperation::next_pane, "F6", "F6", true, true},
    {EditorOperation::close_pane, "Ctrl+Shift+F4", "Ctrl+Shift+F4", true, true},
    {EditorOperation::toggle_preview, "Ctrl+Shift+M", "Ctrl+Shift+M", true, true},
    {EditorOperation::paste_plain, "Ctrl+Shift+V", "Ctrl+Shift+V", true, true},
    // 鍵なし（KeyChord に Alt が無い・ADR 0157 の決定 3）。一覧からだけ。
    {EditorOperation::paste_html, "", "", true, true},
    {EditorOperation::toggle_explorer, "Ctrl+Shift+E", "Ctrl+Shift+E", true, true},
    // 鍵なし。F1 の一覧からだけ（ADR 0162 の決定 7）。
    {EditorOperation::choose_explorer_root, "", "", true, true},
    // F5 の日時の挿入（ADR 0175 の決定 3）。
    {EditorOperation::insert_date_time, "F5", "F5", true, true},
    // 鍵なし。F1 の一覧からだけ（ADR 0010 の追補 #1049 の決定 2 の (c)）。
    {EditorOperation::choose_encoding, "", "", true, true},
    {EditorOperation::choose_line_ending, "", "", true, true},
}};

// 鍵の表示名。全 OperationKey と、Ctrl → Shift → 鍵の並び。
using LabelRow = std::pair<KeyChord, std::string_view>;
constexpr std::array<LabelRow, 27> label_rows{{
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
    {ctrl(OperationKey::space), "Ctrl+Space"},
    {ctrl_shift(OperationKey::space), "Ctrl+Shift+Space"},
    {KeyChord{false, true, OperationKey::f1}, "Shift+F1"},
    {KeyChord{false, false, OperationKey::f6}, "F6"},
    {ctrl(OperationKey::backslash), "Ctrl+\\"},
    {ctrl_shift(OperationKey::backslash), "Ctrl+Shift+\\"},
    {ctrl_shift(OperationKey::f4), "Ctrl+Shift+F4"},
    {ctrl_shift(OperationKey::m), "Ctrl+Shift+M"},
    {ctrl_shift(OperationKey::v), "Ctrl+Shift+V"},
    {ctrl_shift(OperationKey::e), "Ctrl+Shift+E"},
    {KeyChord{false, false, OperationKey::f5}, "F5"},
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
[[nodiscard]] std::vector<Operation>
operations_of(const std::vector<nenenib::core::CommandChoice> &choices)
{
    std::vector<Operation> operations;
    for (const auto &choice : choices)
    {
        operations.push_back(choice.operation);
    }
    return operations;
}

// 空の入力の一覧で operation の行の「名前|説明|鍵」。行が無ければ "<missing>"。
[[nodiscard]] std::string row_text(EditorOperation operation, EditMode mode)
{
    for (const auto &choice : nenenib::core::operation_choices("", mode))
    {
        if (choice.operation == operation)
        {
            const auto detail = choice.detail.has_value() ? choice.detail.value().text() : "";
            return std::string(choice.label.text()) + "|" + std::string(detail) + "|" + choice.key;
        }
    }
    return "<missing>";
}

// 空の入力の一覧で operation の行の鍵の表示名。行が無ければ "<missing>"。
[[nodiscard]] std::string key_for(EditorOperation operation, EditMode mode)
{
    for (const auto &choice : nenenib::core::operation_choices("", mode))
    {
        if (choice.operation == operation)
        {
            return choice.key;
        }
    }
    return "<missing>";
}

// 操作の一覧の候補（ADR 0078 の決定 6・11）。空の入力は表の順でモードの使える操作だけ、入力は
// 名前・読み・鍵の表示名に当てて良い順、どれにも当たらなければ外す。
void verify_operation_choices()
{
    const auto ordinary = nenenib::core::operation_choices("", EditMode::ordinary);
    const auto vim = nenenib::core::operation_choices("", EditMode::vim);
    expect(ordinary.size() == 28 && vim.size() == 26,
           "an empty query lists every operation available in the mode");
    std::vector<Operation> in_order;
    for (const auto &text : nenenib::core::operation_texts)
    {
        in_order.emplace_back(text.operation);
    }
    expect(operations_of(ordinary) == in_order, "an empty query keeps the order of the table");
    expect(std::ranges::none_of(vim,
                                [](const nenenib::core::CommandChoice &choice)
                                {
                                    return choice.operation == EditorOperation::undo ||
                                           choice.operation == EditorOperation::redo;
                                }),
           "the vim list has no undo and no redo");
    expect(std::ranges::all_of(ordinary,
                               [](const nenenib::core::CommandChoice &choice)
                               {
                                   return choice.kind ==
                                              nenenib::core::CommandChoiceKind::operate &&
                                          choice.command.empty() && !choice.origin.has_value() &&
                                          choice.detail.has_value();
                               }),
           "every row operates, carries no command and no origin and shows the description");
    expect(row_text(EditorOperation::save, EditMode::ordinary) ==
               "保存|いまのファイルに上書きします|Ctrl+S",
           "a row shows the name, the description and the key label");
    const auto saved = nenenib::core::operation_choices("ほぞん", EditMode::ordinary);
    expect(operations_of(saved) ==
               std::vector<Operation>{EditorOperation::save, EditorOperation::save_as},
           "the reading finds save and save as, the closer one first");
    const auto keyed = nenenib::core::operation_choices("ctrl+s", EditMode::ordinary);
    expect(keyed.size() >= 2 && keyed.at(0).operation == EditorOperation::save &&
               keyed.at(1).operation == EditorOperation::save_as,
           "the key label finds Ctrl+S before Ctrl+Shift+S");
    expect(operations_of(nenenib::core::operation_choices("f1", EditMode::vim)) ==
               std::vector<Operation>{EditorOperation::list_operations},
           "F1 finds the operation list");
    expect(nenenib::core::operation_choices("qqq", EditMode::ordinary).empty(),
           "a query that hits nothing lists nothing");
    expect(nenenib::core::operation_choices("上書き", EditMode::ordinary).empty(),
           "the description is not matched");
    expect(key_for(EditorOperation::close_tab, EditMode::ordinary) == "Ctrl+W" &&
               key_for(EditorOperation::close_tab, EditMode::vim) == "Ctrl+F4",
           "close tab shows Ctrl+W in the ordinary mode and Ctrl+F4 in vim");
    expect(key_for(EditorOperation::toggle_mode, EditMode::ordinary) == "Ctrl+Shift+Space" &&
               key_for(EditorOperation::toggle_mode, EditMode::vim) == "Ctrl+Shift+Space",
           "the mode toggle shows Ctrl+Shift+Space in both modes");
    expect(key_for(EditorOperation::font_larger, EditMode::ordinary) == "Ctrl++",
           "a key kept with Shift is not the shown key");
}

// F5 の日時の挿入は F1 の一覧に表の行から載る（ADR 0175 の決定 3・ADR 0078）。
void verify_date_time_choice()
{
    expect(row_text(EditorOperation::insert_date_time, EditMode::ordinary) ==
                   "日時を挿入|" +
                       std::string(nenenib::core::operation_text(EditorOperation::insert_date_time)
                                       .description) +
                       "|F5" &&
               key_for(EditorOperation::insert_date_time, EditMode::vim) == "F5",
           "the operation list shows insert date and time with F5 in both modes");
    expect(operations_of(nenenib::core::operation_choices("f5", EditMode::ordinary)) ==
               std::vector<Operation>{EditorOperation::insert_date_time},
           "F5 finds insert date and time");
}
} // namespace

void verify_operations_scope()
{
    verify_operation_for();
    verify_shown_and_available();
    verify_labels();
    verify_no_shared_key();
    verify_texts();
    verify_operation_choices();
    verify_date_time_choice();
}
} // namespace nenenib::tests
