#pragma once

#include "FoundLanguageServer.hpp"
#include "LanguageServerProbe.hpp"

#include <optional>
#include <vector>

namespace nenenib::application
{
// PC の中の言語サーバーを裏で探す口（LSP 段 S4・#1945・ADR 0160 の決定 7 の追補・ADR 0004 の仕事の
// 種類ごとの port・形は FolderPort と同じで、合図を結ぶ bind は adapter が持つ）。環境変数と
// ファイルに触れるのは adapters だけ（ARC-007）。search は頼んですぐ戻る（前の頼みが終わって
// いなければ、新しい頼みの結果だけが残る）。collect は届いた結果を受け取る（まだなら値なし）。
// 結果は probes と同じ順・同じ数で、見つからない候補は値なし。UI スレッドだけが呼ぶ。controller
// が呼ぶのは設定の面を開いたときと、頼んだ後の合図のときだけ（起動・開く・切り替え・保存・打鍵の
// 道では呼ばない・契約 verify_quiet_paths）。
class LanguageServerSearchPort
{
  public:
    LanguageServerSearchPort() = default;
    virtual ~LanguageServerSearchPort() = default;
    LanguageServerSearchPort(const LanguageServerSearchPort &) = delete;
    LanguageServerSearchPort(LanguageServerSearchPort &&) = delete;
    LanguageServerSearchPort &operator=(const LanguageServerSearchPort &) = delete;
    LanguageServerSearchPort &operator=(LanguageServerSearchPort &&) = delete;

    virtual void search(std::vector<LanguageServerProbe> probes) = 0;
    [[nodiscard]] virtual std::optional<std::vector<std::optional<FoundLanguageServer>>>
    collect() = 0;
};
} // namespace nenenib::application
