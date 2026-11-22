#pragma once

#include "LanguageRootAnswer.hpp"
#include "LanguageRootRequest.hpp"

#include <vector>

namespace nenenib::application
{
// 言語サーバーの root を文書のフォルダから上へ印で探す口（ADR 0161 の 2026-11-22 の追補・段 S3・
// ADR 0004 の仕事の種類ごとの port・形は FolderPort と同じで、合図を結ぶ bind は adapter が持つ）。
// ファイルに触れるのは adapters だけで、find は頼んですぐ戻る（ネットワークの道で遅くても UI
// スレッドを止めない）。collect は WorkCompleted の合図の後に終わった頼みの答えを全部返す（無ければ
// 空）。UI スレッドだけが呼ぶ。controller が呼ぶのは lsp がオンで、命令のある言語の文書の root
// がまだ分からないときと、頼んだ後の合図のときだけ。
class LanguageRootPort
{
  public:
    LanguageRootPort() = default;
    virtual ~LanguageRootPort() = default;
    LanguageRootPort(const LanguageRootPort &) = delete;
    LanguageRootPort(LanguageRootPort &&) = delete;
    LanguageRootPort &operator=(const LanguageRootPort &) = delete;
    LanguageRootPort &operator=(LanguageRootPort &&) = delete;

    virtual void find(LanguageRootRequest request) = 0;
    [[nodiscard]] virtual std::vector<LanguageRootAnswer> collect() = 0;
};
} // namespace nenenib::application
