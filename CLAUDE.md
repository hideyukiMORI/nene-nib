# CLAUDE.md — NeNe Nib

Claude Code / AI エージェントがこのリポジトリで作業するための**中核ハンドブック**。
簡潔な英語版の入口は [AGENTS.md](AGENTS.md)。詳細の正本は `docs/` にあり、ここには複製しない。

---

## 0. まず読むもの（production コードに触れる前に必ず）

0. [SPECIFICATION.md](SPECIFICATION.md) — 何を作るか（FR-NNN・施主決定 D1〜D10）
1. [docs/ARCHITECTURE_CONSTITUTION.md](docs/ARCHITECTURE_CONSTITUTION.md) — 憲章（ARC-NNN）
2. [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md) — モジュールと依存方向
3. [docs/CODING_RULES.md](docs/CODING_RULES.md) — C++23 (clang-cl) 規約（CPP-NNN）
4. [docs/QUALITY_GATES.md](docs/QUALITY_GATES.md) — **いま何が機械で守られているか**（QLT-NNN / CNF-NNN）
5. [docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) — 手順
6. [docs/COMMIT_CONVENTIONS.md](docs/COMMIT_CONVENTIONS.md) — Issue・ブランチ・コミット・PR（GIT-NNN）
7. [docs/GLOSSARY.md](docs/GLOSSARY.md) — 用語
8. 該当する ADR（`docs/adr/`）と有効な waiver（`docs/waivers/`）

---

## 1. このリポジトリの統治原則

> **一つのことを実現する方法を 1 つに固定し、そのことを人の記憶ではなく機械に守らせる。**

その帰結として、次の 3 つを常に守る。

1. **正典の経路を先に特定してから編集する。** 「ここで書いたほうが早いから」で第 2 の経路を作らない（ARC-001 / ARC-012）
2. **ゲートを弱めて通さない。** 検査が落ちたらコードを直す。閾値・除外・重大度を触るのは ADR 相当の判断（QLT-010）
3. **`planned` を `active` と書かない。** 未実装の強制を実装済みに見せるのは、この規約体系で唯一「壊す」行為（[ADR 0001](docs/adr/0001-strictness-is-mechanically-enforced.md)）

---

## 2. このプロジェクトで間違えやすい所

### コンパイラは clang-cl だけ

`cl` は Phase 0 の比較対象としてだけ残っている（[ADR 0003](docs/adr/0003-cpp23-clang-cl-foundation-and-measured-limits.md)）。`eng/toolchain.ps1` が `CXX=clang-cl` を固定する。
製品も測定ビルドも将来の md4c も同じ 1 本。警告集合は `eng/targets.cmake` の 1 か所で、`eng/probes/language.json` の `clangStrict` と同じ並び。

### 現在時刻・ファイル・スレッドを持てる場所は 1 つしかない

現在時刻・乱数・既定ロケール・環境変数・ファイル・**スレッド**を持ってよいのは **`src/adapters/win32`** だけである（ARC-007 / ARC-003 / CPP-013）。
中核で必要なら、型のあるポートから注入する。**テストが実時刻を読むことも決定性の破壊である。**
検査はソースの名前ではなく **リンカのシンボル**で行う（`eng/symbols.py`）。`system_clock::now()` は `_Xtime_get_ticks`、`std::thread` は `_beginthreadex` として現れる。
`std::atomic` はリンカに見えないので、`<atomic>` の include を字句検査（CNF-009）が拒否する。

### 網羅性検査を殺す分岐を書かない

閉じた選択肢の分岐に `default` / `else` / `_` を書かない。選択肢が増えたらコンパイルが落ちるのが正しい状態（CPP-002）。
`DefWindowProcW` へ渡す OS メッセージの既定分岐だけは例外（CPP-017）。

### 期待される失敗は例外にしない

検証エラー・見つからない・拒否・非互換・device lost は `std::expected` か閉じた `enum class` で返し、`[[nodiscard]]` を付ける（ARC-010 / CPP-005）。

### `std::optional` は `value()` で読む

MSVC STL の `optional` は `operator*` を clang-tidy が見ない（Phase 0 の T3-tidy-unchecked-optional-star-hole）。`value()` / `value_or()` で読み、`*` と `->` を書かない（CPP-004）。

### COM は `-Wno-language-extension-token` の上で書く

`__uuidof` / `IID_PPV_ARGS` は `-Wpedantic` で落ちる（W1）。名指しで外してあるので、警告集合を自分で触らない。COM の所有は `ComPtr`、device lost は結果型（CPP-017）。

### SIMD は `src/core/simd/` にだけ書く

関数ごとに `[[gnu::target("…")]]` を付け、fallback を必ず置く。target 単位の `/arch` は付けない（CPP-018・[ADR 0006](docs/adr/0006-speed-gate-simd-and-table-driven-dispatch.md)）。

### 大きな分岐は表で書く

60 分岐の `switch` は関数長 60 行で落ち、`constexpr` の表は通る（T8）。Vim のキー列 → 動作は表（CPP-012）。

### 公開 aggregate に `= default` の `operator==` を書かない

`RgbColor` / `Palette` のような公開 aggregate はメソッドを 1 つでも持つと lint が落ちる（Issue #3）。比較は非メンバーで書く（CPP-003）。

### `reinterpret_cast` は書けない。`std::bit_cast` と `ComPtr<IUnknown>` ＋ `As()`

clang-tidy が一律に拒否する。HWND ↔ `this` と `LPARAM` の読み替えは `std::bit_cast`、`IUnknown**` を要求する API は `ComPtr<IUnknown>` で受けて `As()`（CPP-009）。

### `.cpp` の無名名前空間の `struct` も 1 ファイル 1 型に数える

ローカルの補助は自由関数と `using` 別名で書く（CPP-011）。

### ui/win32 に色のリテラルを書かない

色は `core::Palette` のトークンだけ（テーマ拡張の前提・ADR 0008 決定 8）。足りない色はトークンを 1 つ足して採用案の表に書く。

### Mica には `WS_EX_NOREDIRECTIONBITMAP` と `DWMWA_USE_IMMERSIVE_DARK_MODE` が要る

無いとタイトルバーに透けない・ダークでも明るいままになる（Issue #5 で実測。ADR 0008 決定 9）。

### core で `std::lround` を呼ばない

libm のシンボルが core の外へ出て ARC-003 が落ちる。DIP → 物理画素は整数演算（ADR 0008 決定 5）。

### `std::string_view::find` などの STL の検索は許可シンボルに載っている

MSVC STL は `__std_find_trivial_*` 等の純関数を core の外へ出す（Issue #7 で実測）。`eng/symbol-allowlist.json` に足してある。他の `__std_*` が出たら「時刻・OS・スレッドに触れないか」を確かめてから足す。

### 意図は `std::variant` の閉じた和型

`EditorIntent` は `std::visit` で写し、選択肢が増えたらコンパイルが落ちる（ADR 0009）。方向や操作の種類は閉じた `enum` を持つ型に畳む。

### md4c はまだ入っていない

Markdown プレビュー（FR-007）の Issue で、別 target に `/W4 /WX` だけを当てて tag と SHA-256 で固定して入れる（ADR 0003）。厳格集合を当てると 20 件超で落ちる。

---

## 3. 検証コマンド

```bash
pwsh -NoProfile -File ./eng/check.ps1          # 唯一の完了定義（ローカルと CI で同じ）
```

開発中は最も狭い検査を使ってよい。フルゲートは **PR を Draft → Ready にする直前**に必ず通す。

🔴 **`pwsh -NoProfile -File ./eng/check.ps1` が通っていないものを「できた」と報告しない。**
実行していないコマンドの結果を書かない。テストの失敗を隠さない。
テストが本当の欠陥を見つけたら、期待値ではなく production コードを直す。

---

## 4. 変更の進め方

[docs/DEVELOPMENT_WORKFLOW.md](docs/DEVELOPMENT_WORKFLOW.md) が正本。要約すると:

Issue → 正典経路の特定 → ブランチ → （設計を変えるなら先に ADR）→ 最小の実装 →
テスト → 狭い検査 → `pwsh -NoProfile -File ./eng/check.ps1` → 規則 ID ごとの自己レビュー → PR（draft）→ Ready → squash merge。

コミットは Conventional Commits（`type` と `scope` は英語、説明は日本語、末尾に `(#N)`）。形の正本は GIT-003。

---

## 5. 完了報告の形

作業を終えたら必ず次を報告する。

```text
Issue / 規則 ID:
変更したファイルと振る舞い:
実行した検証コマンドと結果:
ドキュメント・スキーマの変更:
Waivers: none | WVR-NNNN
残るリスク:
```

調査だけを頼まれたときは、編集・コミット・push・PR 作成・外部状態の変更を行わない。

---

## 6. いまの状況

現在のタスクは [docs/todo/current.md](docs/todo/current.md)。GitHub Issue が正で、そこは要約。

2026-09-15: Issue #1（Phase 0〜2）、#3（最初の縦切り・ADR 0007）、#5（見た目・ADR 0008）、#7（編集・ADR 0009）。起動すると枠なし窓（Snap と影は OS のまま）に Mica のタイトルバー、タブ 1 本と窓の操作、
piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）、ステータスバーの「通常 | Vim」トグルを Direct2D で描き、OS のライト／ダーク（茄子色 D11・橙 D12）に従う。
見た目の正本は `docs/design/2026-09-15-look.md` と `docs/design/2026-09-15-editing-look.md`。カラーテーマとフォントサイズの計画は `docs/plans/2026-09-15-colorschemes.md`（D13 / D14）。
ファイルの開閉と保存・IME・Vim エンジン・複数タブ・Ctrl+P・設定の保存はまだ無い。最新は [日報](docs/reports/2026-09-15.md) / [引き継ぎ](docs/handoffs/2026-09-15.md)。

---

## 7. 上位のポリシーと雛形の所在

このリポジトリの規約は **AYANE 厳格規約ポリシー**の 3 回目の適用例である（1 回目 nene-loupe・C++23 / MSVC、2 回目 nene-folio・C23 / clang-cl）。
ポリシー本文・初期化手順・雛形は施主の private ワークスペースにあり、このリポジトリには置かない。
雛形で足りなかったものを見つけたら、このリポを直すと同時に施主へ還流点として報告する。
