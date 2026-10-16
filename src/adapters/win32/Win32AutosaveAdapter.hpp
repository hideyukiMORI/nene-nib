#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveOutcome.hpp"
#include "AutosavePort.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveShelf.hpp"
#include "AutosaveSnapshot.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "Win32Worker.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::adapters::win32
{
// 自動退避の置き場（ADR 0137 の決定 4）。設定と同じ親の `autosave`。
[[nodiscard]] std::optional<core::FilePath> local_autosave_root();

// 未保存の編集を裏で退避する（ADR 0137 の決定 4〜6）。置き場は root の下の自分の pid の
// 小文字 16 進のフォルダで、中身は一覧 `autosave.v1` と本文 `<番号>.body`。どちらも files の
// 原子的な書き（一時ファイル → 置き換え・ADR 0010 / 0135）で、ワーカーのスレッドから書く。
// 一覧は自分のワーカーだけが触るので、読む → 直す → 書く。store と discard の結果は成功でも
// 失敗でも id つきで棚にたまり、bind された合図を仕事が 1 つ終わるたびに 1 回呼ぶ。
// HWND も窓メッセージの番号も知らない。root が無ければ store は location_unavailable で失敗する。
class Win32AutosaveAdapter final : public application::AutosavePort
{
  public:
    Win32AutosaveAdapter(Win32Worker &worker, application::FilePort &files,
                         std::optional<core::FilePath> root);
    ~Win32AutosaveAdapter() override;
    Win32AutosaveAdapter(const Win32AutosaveAdapter &) = delete;
    Win32AutosaveAdapter(Win32AutosaveAdapter &&) = delete;
    Win32AutosaveAdapter &operator=(const Win32AutosaveAdapter &) = delete;
    Win32AutosaveAdapter &operator=(Win32AutosaveAdapter &&) = delete;

    // 合図はワーカーのスレッドから、store か discard の仕事が 1 つ終わるたびに呼ばれる。
    void bind(std::function<void()> finished);
    void store(application::AutosaveSnapshot snapshot) override;
    void discard(application::AutosaveId id) override;
    [[nodiscard]] std::vector<application::AutosaveOutcome> collect() override;
    void clear() override;
    [[nodiscard]] std::expected<std::vector<application::AutosaveRemnant>,
                                application::AutosaveFailure>
    remnants() override;
    [[nodiscard]] std::expected<std::vector<std::byte>, application::AutosaveFailure>
    read(const application::AutosaveRemnant &remnant) override;
    void forget(const application::AutosaveRemnant &remnant) override;

  private:
    Win32Worker &worker_;
    application::FilePort &files_;
    std::optional<core::FilePath> root_;
    std::shared_ptr<AutosaveShelf> shelf_;
};
} // namespace nenenib::adapters::win32
