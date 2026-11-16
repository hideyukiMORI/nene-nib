#pragma once

#include "FileHandle.hpp"
#include "FilePath.hpp"
#include "ProjectScan.hpp"
#include "ProjectSearchHit.hpp"
#include "ProjectSearchPhase.hpp"
#include "ProjectSearchRequest.hpp"
#include "ProjectSearchShelf.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 1 つの要求の全文検索（ADR 0167 の決定 2〜4）。worker の上で走り、1 回の run で 1 つの仕事
// （フォルダ 1 つの列挙か、ファイルの塊 1 つの読みと照合）だけをして、残りがあれば自分を worker の
// 列の後ろへ積み直す（FIFO を塞がない・自動退避や device の仕事が塊の間に入れる）。
// - 木はルートから歩く。フォルダは見つけた順に深さ優先、フォルダのファイルはその下のフォルダより
//   先に読む。project_folder_walked が断るフォルダ（隠し属性・ジャンクション・シンボリック
//   リンク）と core::project_folder_skipped の表の名前は歩かない。開けないフォルダ
//   （拒否・長すぎる経路）は黙って飛ばす。ルートを開けなければ failed。
// - ファイルは中身が手元に無いもの（project_file_read）と core::project_file_fits を超えるものを
//   数えずに飛ばし、最初の塊に NUL があれば
//   （core::project_text_is_binary）・開けない・読めなければ、見つけた数から外して飛ばす。
// - 読みは core::project_search_chunk_bytes ずつ。照合は core::project_hits_in と
//   project_hits_finished（行の境と BOM は core が扱う）。当たりのファイルのパスはルートの文字列の
//   後ろに `\` と名前を足した形（区切りで終わるルートには足さない・FolderListing と同じ）。
// - 当たりが core::project_search_hit_limit に届いたら truncated で終える。
// 進んだ仕事の後に batch（当たりと累計の進み具合）を 1 つ置く。止める合図か新しい世代を見たら
// 何も置かずに戻る。やめさせられない限り、最後の batch（complete・truncated・failed）を必ず
// 1 つ出す。move だけできる（読んでいるファイルのハンドルを 1 つ持つ）。
class ProjectSearchWalk final
{
  public:
    ProjectSearchWalk(application::ProjectSearchRequest request, std::uint64_t generation,
                      std::shared_ptr<ProjectSearchShelf> shelf, Win32Worker &worker);

    void run(const std::stop_token &stop);

  private:
    [[nodiscard]] bool live(const std::stop_token &stop) const;
    // 仕事を 1 つする。終わったら最後の段を返す。
    [[nodiscard]] std::optional<application::ProjectSearchPhase> step();
    // フォルダ 1 つを列挙して、歩くフォルダと読むファイルを足す。ルートを開けなければ failed。
    [[nodiscard]] std::optional<application::ProjectSearchPhase> list_folder();
    void take_entry(const core::FilePath &folder, const WIN32_FIND_DATAW &data,
                    std::vector<core::FilePath> &folders);
    // 次のファイルを開いて最初の塊を読む。開けなければ飛ばす。
    [[nodiscard]] std::optional<application::ProjectSearchPhase> open_next();
    // 開いているファイルの塊 1 つを読んで照合する。
    [[nodiscard]] std::optional<application::ProjectSearchPhase> read_chunk();
    // 読み終えたファイルの残りの行を照合して閉じる。
    [[nodiscard]] std::optional<application::ProjectSearchPhase> end_file();
    // 読めない・バイナリのファイルを閉じて、見つけた数から外す。
    void skip_file();
    // core の当たりを足す。上限に届いたら truncated。
    [[nodiscard]] std::optional<application::ProjectSearchPhase>
    add_hits(const std::vector<core::ProjectHit> &hits);
    [[nodiscard]] std::size_t room() const noexcept;
    [[nodiscard]] bool send(application::ProjectSearchPhase phase);

    application::ProjectSearchRequest request_;
    std::uint64_t generation_;
    std::shared_ptr<ProjectSearchShelf> shelf_;
    Win32Worker *worker_;
    // 歩くフォルダ（末尾から取る）と、列挙したフォルダの読むファイル（先頭から取る）。
    std::vector<core::FilePath> folders_;
    std::deque<core::FilePath> files_;
    bool root_listed_ = false;
    // 読んでいるファイル（無ければ null）とその途中の状態。
    std::unique_ptr<FileHandle> file_;
    std::optional<core::FilePath> reading_;
    core::ProjectScan scan_;
    bool first_chunk_ = false;
    std::string buffer_;
    // 次の batch に載せる当たりと、要求からの累計。
    std::vector<application::ProjectSearchHit> hits_;
    std::size_t total_hits_ = 0;
    std::size_t searched_ = 0;
    std::size_t found_ = 0;
    // 前の batch の後に進んだか（当たり・数）。
    bool moved_ = false;
};
} // namespace nenenib::adapters::win32
