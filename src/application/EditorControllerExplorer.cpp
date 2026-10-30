#include "EditorController.hpp"

#include "EditorControllerDetail.hpp"
#include "ExplorerEntry.hpp"
#include "ExplorerFolder.hpp"
#include "ExplorerNode.hpp"
#include "ExplorerRow.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerState.hpp"
#include "ExplorerStep.hpp"
#include "ExplorerTree.hpp"
#include "ExplorerView.hpp"
#include "FileFolder.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderListingKind.hpp"
#include "FolderProgress.hpp"
#include "FolderRequest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
// 読めないフォルダの知らせ（ADR 0162 の決定 5）。名前の後ろに付ける。
constexpr std::string_view unreadable_suffix = "（読めません）";
constexpr std::string_view root_prefix = "root=";

// 面の見えている行の先頭の上限（行の数 rows と面の行数 page から）。
[[nodiscard]] std::size_t last_first(std::size_t rows, std::size_t page) noexcept
{
    return rows > page ? rows - page : 0;
}

// 選択を行の範囲へ寄せ、先頭を上限へ寄せる（選択を見える所へは送らない・裏で届いた行と
// ホイールの道）。
[[nodiscard]] ExplorerState clamped(ExplorerState explorer, std::size_t rows, std::size_t page)
{
    if (rows == 0)
    {
        explorer.selected = std::nullopt;
        explorer.first_visible = 0;
        return explorer;
    }
    if (explorer.selected.has_value())
    {
        explorer.selected = std::min(explorer.selected.value(), rows - 1);
    }
    explorer.first_visible = std::min(explorer.first_visible, last_first(rows, page));
    return explorer;
}

// clamped の後に、選択が面の見えている行に入るよう先頭を送る（鍵とクリックの道）。
[[nodiscard]] ExplorerState followed(ExplorerState explorer, std::size_t rows, std::size_t page)
{
    explorer = clamped(std::move(explorer), rows, page);
    if (!explorer.selected.has_value())
    {
        return explorer;
    }
    const std::size_t selected = explorer.selected.value();
    if (selected < explorer.first_visible)
    {
        explorer.first_visible = selected;
    }
    else if (selected >= explorer.first_visible + page)
    {
        explorer.first_visible = selected - page + 1;
    }
    return explorer;
}

// j k の 1 歩。選択が無ければ先頭の行。端では止まる。
[[nodiscard]] std::size_t stepped(std::optional<std::size_t> selected, std::size_t rows,
                                  bool down) noexcept
{
    if (!selected.has_value())
    {
        return 0;
    }
    const std::size_t row = selected.value();
    if (down)
    {
        return std::min(row + 1, rows - 1);
    }
    return row == 0 ? 0 : row - 1;
}

// 行 row の節の経路（「…」の行と範囲の外は値なし）。節の番号は前の行に子が届くと変わるので、
// 行をまたいで同じ節を指すときは経路で覚える。
[[nodiscard]] std::optional<core::FilePath> path_at(const core::ExplorerTree &tree,
                                                    std::span<const core::ExplorerRow> rows,
                                                    std::optional<std::size_t> row)
{
    if (!row.has_value() || row.value() >= rows.size() || rows[row.value()].more)
    {
        return std::nullopt;
    }
    return tree.path_of(rows[row.value()].node);
}

// 経路 path の節の行。見えていなければ値なし。
[[nodiscard]] std::optional<std::size_t> row_of(const core::ExplorerTree &tree,
                                                std::span<const core::ExplorerRow> rows,
                                                const core::FilePath &path)
{
    const auto node = tree.node_of(path);
    if (!node.has_value())
    {
        return std::nullopt;
    }
    const auto found = std::ranges::find_if(rows, [&](const core::ExplorerRow &row)
                                            { return !row.more && row.node == node.value(); });
    if (found == rows.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - rows.begin());
}

[[nodiscard]] std::string shown_name(const core::FilePath &folder)
{
    const std::string_view name = folder.file_name();
    return std::string(name.empty() ? folder.text() : name);
}
} // namespace

// ---------------------------------------------------------------- エクスプローラー（ADR 0162）

// D66 の規則（決定 4）。出す・消すは設定 explorer を書く（Ex の `set explorer` と同じ
// persist_settings の 1 本・焦点だけのときは書かない）。消したときの焦点は sync_explorer が外す。
void EditorController::accept(const ToggleExplorer &)
{
    auto settings = state_.settings();
    if (!settings.explorer)
    {
        settings.explorer = true;
        if (persist_settings(std::move(settings)))
        {
            focus_explorer(true);
        }
        return;
    }
    if (state_.explorer().focused)
    {
        settings.explorer = false;
        static_cast<void>(persist_settings(std::move(settings)));
        return;
    }
    focus_explorer(true);
}

// 面の鍵（決定 4）。j / k / ↓ / ↑ は選ぶ、l / → はフォルダを開く、Enter はフォルダを開くか
// ファイルを開いて本文へ焦点を戻す、h / ← は閉じる（閉じていれば親へ）、Esc は焦点だけ戻す。
// ルートの無い面では行が無いので、Esc のほかは何も動かさない。
void EditorController::accept(const ExplorerKey &intent)
{
    if (!state_.settings().explorer)
    {
        return;
    }
    switch (intent.key)
    {
    case ExplorerKeyKind::j:
    case ExplorerKeyKind::down:
        step_explorer(true);
        return;
    case ExplorerKeyKind::k:
    case ExplorerKeyKind::up:
        step_explorer(false);
        return;
    case ExplorerKeyKind::l:
    case ExplorerKeyKind::right:
        open_explorer_folder(state_.explorer().selected);
        return;
    case ExplorerKeyKind::enter:
        enter_explorer_row(state_.explorer().selected);
        return;
    case ExplorerKeyKind::h:
    case ExplorerKeyKind::left:
        collapse_explorer_row();
        return;
    case ExplorerKeyKind::esc:
        focus_explorer(false);
        return;
    }
}

// 面の行の上のマウス（決定 4・ADR 0056 の決定 9）。押すと面に焦点を置いて行を覚え、離した行が
// 同じときだけ選ぶ（フォルダなら開閉）。ダブルクリックは開く。
void EditorController::accept(const PointExplorer &intent)
{
    if (!state_.settings().explorer)
    {
        return;
    }
    const auto &tree = state_.explorer().tree;
    const auto rows = tree.visible_rows();
    switch (intent.pointing)
    {
    case ExplorerPointing::pressed:
        // 押した行は節の経路で覚える（押してから離すまでに子が届いて行がずれても取り違えない）。
        explorer_pressed_ = path_at(tree, rows, intent.row);
        focus_explorer(true);
        return;
    case ExplorerPointing::released:
        release_explorer(path_at(tree, rows, intent.row), intent.row);
        return;
    case ExplorerPointing::double_click:
        explorer_pressed_.reset();
        if (intent.row < rows.size())
        {
            select_explorer_row(intent.row);
            enter_explorer_row(intent.row);
        }
        return;
    }
}

// 離した行の節 released が押した節と同じときだけ 1 回のクリックにする。押した印は消す。
void EditorController::release_explorer(const std::optional<core::FilePath> &released,
                                        std::size_t row)
{
    const auto pressed = std::exchange(explorer_pressed_, std::nullopt);
    if (pressed.has_value() && released == pressed)
    {
        click_explorer_row(row);
    }
}

// 面のホイール（決定 4）。先頭だけを送り、行数で切り詰める。選択は動かさない。
void EditorController::accept(const ScrollExplorer &intent)
{
    if (!state_.settings().explorer)
    {
        return;
    }
    ExplorerState explorer = state_.explorer();
    const std::size_t rows = explorer.tree.visible_rows().size();
    const auto last = static_cast<std::int64_t>(last_first(rows, explorer_page()));
    const auto moved = static_cast<std::int64_t>(explorer.first_visible) + intent.lines;
    explorer.first_visible = static_cast<std::size_t>(std::clamp<std::int64_t>(moved, 0, last));
    state_ = state_.with_explorer(std::move(explorer));
}

// 操作「ルートのフォルダを選ぶ」の答え（決定 7）。`:set root=<path>` と同じく設定へ書く。
void EditorController::accept(const ExplorerRootChosen &intent)
{
    set_explorer_root(intent.folder);
}

// `:set root=.`（決定 7）。アクティブな文書のフォルダをルートにする。無題なら失敗の 1 行。
void EditorController::run_root_request(core::ExRootRequest request)
{
    switch (request)
    {
    case core::ExRootRequest::active_document_folder:
        break;
    }
    const auto &path = state_.document().path;
    const auto folder = path.has_value() ? core::folder_of(path.value()) : std::nullopt;
    if (!folder.has_value())
    {
        reject(core::DisplayText::parse("E32: No file name").value());
        return;
    }
    set_explorer_root(folder.value());
}

void EditorController::set_explorer_root(const core::FilePath &folder)
{
    auto settings = state_.settings();
    settings.root = folder;
    if (!persist_settings(std::move(settings)))
    {
        reject(core::DisplayText::parse("Settings could not be saved").value());
        return;
    }
    state_ = state_.with_command_message(
        detail::named_line(root_prefix, std::string(folder.text()), {}));
}

void EditorController::focus_explorer(bool focused)
{
    if (state_.explorer().focused == focused)
    {
        return;
    }
    ExplorerState explorer = state_.explorer();
    explorer.focused = focused;
    state_ = state_.with_explorer(std::move(explorer));
}

std::size_t EditorController::explorer_page() const noexcept
{
    // 面の先頭の行はルート名（採用案の表）。面の行数は窓の client の高さから窓が数えた面の行数
    // （VisibleLines.explorer_lines）から導き、本文の分割（ADR 0148）で変わらない（#1125・
    // ADR 0162 の追補）。寸法が届く前（0）だけ本文の見えている行数を面の行数とみなす。
    const std::size_t lines = explorer_lines_ > 0 ? explorer_lines_ : state_.scroll().visible_lines;
    return std::max<std::size_t>(lines, 2) - 1;
}

bool EditorController::explorer_focused() const noexcept
{
    return state_.settings().explorer && state_.explorer().focused;
}

void EditorController::step_explorer(bool down)
{
    ExplorerState explorer = state_.explorer();
    const std::size_t rows = explorer.tree.visible_rows().size();
    if (rows == 0)
    {
        return;
    }
    explorer.selected = stepped(explorer.selected, rows, down);
    state_ = state_.with_explorer(followed(std::move(explorer), rows, explorer_page()));
}

void EditorController::select_explorer_row(std::size_t row)
{
    ExplorerState explorer = state_.explorer();
    const std::size_t rows = explorer.tree.visible_rows().size();
    explorer.selected = row;
    state_ = state_.with_explorer(followed(std::move(explorer), rows, explorer_page()));
}

// 1 回のクリック（決定 4）。選ぶだけで、フォルダなら開閉する。
void EditorController::click_explorer_row(std::size_t row)
{
    select_explorer_row(row);
    ExplorerState explorer = state_.explorer();
    const auto rows = explorer.tree.visible_rows();
    if (!rows[row].folder)
    {
        return;
    }
    explorer.tree = explorer.tree.toggled(row);
    const std::size_t count = explorer.tree.visible_rows().size();
    state_ = state_.with_explorer(followed(std::move(explorer), count, explorer_page()));
    request_explorer_folder();
}

// 閉じたフォルダの行なら開いて、まだ読んでいなければ裏で読む（決定 5）。ほかの行は何もしない。
void EditorController::open_explorer_folder(std::optional<std::size_t> row)
{
    ExplorerState explorer = state_.explorer();
    const auto rows = explorer.tree.visible_rows();
    if (!row.has_value() || row.value() >= rows.size() || !rows[row.value()].folder ||
        rows[row.value()].open)
    {
        return;
    }
    explorer.tree = explorer.tree.toggled(row.value());
    const std::size_t count = explorer.tree.visible_rows().size();
    state_ = state_.with_explorer(followed(std::move(explorer), count, explorer_page()));
    request_explorer_folder();
}

// Enter とダブルクリック（決定 4・6）。フォルダは開き、ファイルは一覧から開くのと同じ open_listed
// の 道で開いて本文へ焦点を戻す（面は残す・失敗は 1 行・履歴は触らない）。
void EditorController::enter_explorer_row(std::optional<std::size_t> row)
{
    const ExplorerState &explorer = state_.explorer();
    const auto rows = explorer.tree.visible_rows();
    if (!row.has_value() || row.value() >= rows.size() || rows[row.value()].more)
    {
        return;
    }
    if (rows[row.value()].folder)
    {
        open_explorer_folder(row);
        return;
    }
    const auto path = explorer.tree.path_of(rows[row.value()].node);
    if (!path.has_value())
    {
        return;
    }
    focus_explorer(false);
    open_listed(path.value(), std::nullopt);
}

void EditorController::collapse_explorer_row()
{
    ExplorerState explorer = state_.explorer();
    if (!explorer.selected.has_value())
    {
        return;
    }
    auto step = explorer.tree.collapsed_to_parent(explorer.selected.value());
    explorer.tree = std::move(step.tree);
    explorer.selected = step.row;
    const std::size_t count = explorer.tree.visible_rows().size();
    state_ = state_.with_explorer(followed(std::move(explorer), count, explorer_page()));
}

// 設定が変わった後と起動の 1 か所（決定 3）。ルートが変われば木を作り直して選択と券を捨て、
// 面を消したら焦点を本文へ戻し、出ていれば開いたまま読んでいないフォルダを頼む。
void EditorController::sync_explorer()
{
    const auto &settings = state_.settings();
    ExplorerState explorer = state_.explorer();
    bool changed = false;
    if (explorer.tree.root() != settings.root)
    {
        explorer =
            ExplorerState{settings.root.has_value() ? core::ExplorerTree(settings.root.value())
                                                    : core::ExplorerTree{},
                          std::nullopt, explorer.focused, 0};
        explorer_folder_.reset();
        explorer_pressed_.reset();
        changed = true;
    }
    if (!settings.explorer && explorer.focused)
    {
        explorer.focused = false;
        changed = true;
    }
    if (changed)
    {
        state_ = state_.with_explorer(std::move(explorer));
    }
    request_explorer_folder();
}

// FolderPort は新しい要求で前の要求をやめる（ADR 0062 の決定 6）ので、エクスプローラーは port が
// 今の誰かの仕事を走らせていない間だけ頼む（1 つずつ）。今の仕事はエクスプローラーの控えか、
// 開いている Ctrl+P の面の券。それ以外（ルートを替えて捨てた控え・閉じた面の券）は上書きしてよい。
bool EditorController::folder_busy() const noexcept
{
    if (!folder_live_.has_value())
    {
        return false;
    }
    const std::uint64_t live = folder_live_.value();
    if (explorer_folder_.has_value() && explorer_folder_.value().ticket == live)
    {
        return true;
    }
    return live == palette_folder_.ticket && command_palette_active();
}

// 開いたまま読んでいないフォルダを 1 つ頼む（決定 5）。木の節の順で最初のもの。面が消えている・
// port が仕事中・読むものが無いなら頼まない。打鍵・切り替え・保存の道は呼ばない。
void EditorController::request_explorer_folder()
{
    if (!state_.settings().explorer || folder_busy())
    {
        return;
    }
    const auto &tree = state_.explorer().tree;
    const auto nodes = tree.nodes();
    const auto waiting = std::ranges::find_if(nodes, [](const core::ExplorerNode &node)
                                              { return node.folder && node.open && !node.loaded; });
    if (waiting == nodes.end())
    {
        return;
    }
    const auto folder = tree.path_of(static_cast<std::size_t>(waiting - nodes.begin()));
    if (!folder.has_value())
    {
        return;
    }
    const std::uint64_t ticket = ++folder_tickets_;
    ports_.folders.list(FolderRequest{folder.value(), ticket, FolderListingKind::entries});
    folder_live_ = ticket;
    explorer_folder_ = ExplorerFolder{ticket, folder.value(), {}};
}

// FolderPort の collect の 1 本（ADR 0062 の決定 15・ADR 0162 の決定 5）。今の仕事の最後の batch で
// 仕事中の印を外し、エクスプローラーの控えの券の batch を木へ写す。面の分を含めて全部を返す。
std::vector<FolderBatch> EditorController::receive_folders()
{
    auto batches = ports_.folders.collect();
    for (FolderBatch &batch : batches)
    {
        if (folder_live_ == std::optional{batch.ticket} && batch.progress != FolderProgress::more)
        {
            folder_live_.reset();
        }
        if (explorer_folder_.has_value() && explorer_folder_.value().ticket == batch.ticket)
        {
            adopt_explorer_batch(explorer_folder_.value(), batch);
        }
    }
    return batches;
}

// pending は控え（explorer_folder_ の中身）。最後の batch で木へ置いて控えを消す。
void EditorController::adopt_explorer_batch(ExplorerFolder &pending, FolderBatch &batch)
{
    for (FolderEntry &entry : batch.entries)
    {
        pending.entries.push_back(
            core::ExplorerEntry{std::string(entry.path.file_name()), entry.folder, entry.hidden});
    }
    switch (batch.progress)
    {
    case FolderProgress::more:
        return;
    case FolderProgress::complete:
    case FolderProgress::truncated:
        place_explorer_entries(pending.folder, pending.entries);
        break;
    case FolderProgress::failed:
        // 行は残し（子の無い読み終えたフォルダにして頼み直さない）、1 行だけ知らせる。
        place_explorer_entries(pending.folder, {});
        state_ = state_.with_command_message(
            detail::named_line({}, shown_name(pending.folder), unreadable_suffix));
        break;
    }
    explorer_folder_.reset();
}

// 届いた子を木へ置く。節は経路で引き直し（前に置いた子で番号が変わる）、選んでいた節を選び続ける。
void EditorController::place_explorer_entries(const core::FilePath &folder,
                                              std::span<const core::ExplorerEntry> entries)
{
    ExplorerState explorer = state_.explorer();
    const auto node = explorer.tree.node_of(folder);
    if (!node.has_value())
    {
        return;
    }
    const auto kept = path_at(explorer.tree, explorer.tree.visible_rows(), explorer.selected);
    explorer.tree = explorer.tree.loaded(node.value(), entries);
    const auto rows = explorer.tree.visible_rows();
    if (kept.has_value())
    {
        explorer.selected =
            row_of(explorer.tree, rows, kept.value()).value_or(explorer.selected.value_or(0));
    }
    state_ = state_.with_explorer(clamped(std::move(explorer), rows.size(), explorer_page()));
}

// 面の表示値（決定 3・8）。面が消えていれば値なし（分岐 1 つ）。名前を写すのは first_visible から
// 面の行数（explorer_page）ぶんだけで、frame の費用は木の大きさではなく画面の行数に依る。
std::optional<ExplorerView> EditorController::explorer_view() const
{
    if (!state_.settings().explorer)
    {
        return std::nullopt;
    }
    const ExplorerState &explorer = state_.explorer();
    const auto nodes = explorer.tree.nodes();
    const auto rows = explorer.tree.visible_rows();
    const std::size_t first = std::min(explorer.first_visible, rows.size());
    const std::size_t last = std::min(first + explorer_page(), rows.size());
    std::vector<ExplorerRowView> views;
    views.reserve(last - first);
    for (std::size_t index = first; index < last; ++index)
    {
        const core::ExplorerRow &row = rows[index];
        views.push_back(ExplorerRowView{row.more ? std::string{} : nodes[row.node].name, row.depth,
                                        row.folder, row.open, row.hidden, row.more});
    }
    return ExplorerView{explorer.tree.root(), std::move(views), rows.size(),
                        explorer.selected,    explorer.focused, first};
}
} // namespace nenenib::application
