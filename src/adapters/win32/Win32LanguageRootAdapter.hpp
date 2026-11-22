#pragma once

#include "FilePath.hpp"
#include "Language.hpp"
#include "LanguageRootAnswer.hpp"
#include "LanguageRootPort.hpp"
#include "LanguageRootRequest.hpp"
#include "LanguageRootShelf.hpp"
#include "Win32Worker.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace nenenib::adapters::win32
{
// 頼みのフォルダから上へ（頼みの上限があればそれより上へは出ない・#1960）、言語の印のどれかがある
// 最初のフォルダ（段 S3）。印の有る無しは GetFileAttributesW の 1 回ずつで見る（ファイルでも
// フォルダでもよい・`.git` は worktree ではファイル）。stop が来たら以後は見ずに値なし。
// ワーカーのスレッドで呼ぶ。
[[nodiscard]] std::optional<core::FilePath>
language_root_on_disk(const application::LanguageRootRequest &request, const std::stop_token &stop);

// 言語サーバーの root を探す本物（ADR 0161 の 2026-11-22 の追補・段 S3・Win32RevealAdapter と同じ
// 形）。探すのはワーカーを借りて 1 頼みずつ走り（UI スレッドを止めない）、答えは棚にたまる。
// 「届いた」の合図は bind で後から受け取る（渡されるまでは知らせない）。HWND も窓メッセージの番号も
// 知らない。
class Win32LanguageRootAdapter final : public application::LanguageRootPort
{
  public:
    explicit Win32LanguageRootAdapter(Win32Worker &worker);
    ~Win32LanguageRootAdapter() override;
    Win32LanguageRootAdapter(const Win32LanguageRootAdapter &) = delete;
    Win32LanguageRootAdapter(Win32LanguageRootAdapter &&) = delete;
    Win32LanguageRootAdapter &operator=(const Win32LanguageRootAdapter &) = delete;
    Win32LanguageRootAdapter &operator=(Win32LanguageRootAdapter &&) = delete;

    // 合図はワーカーのスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void find(application::LanguageRootRequest request) override;
    [[nodiscard]] std::vector<application::LanguageRootAnswer> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<LanguageRootShelf> shelf_;
};
} // namespace nenenib::adapters::win32
