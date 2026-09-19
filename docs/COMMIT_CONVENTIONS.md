# コミットとリポジトリの規約 — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版
> 出典: NeNeCommander `docs/COMMIT_CONVENTIONS.md`（GIT-001〜004・`commit-msg` フックで機械強制）

変更の経路は一つである: Issue → 焦点を持つブランチ → 一貫したコミット → PR → 必須 check → squash merge → 同期した綺麗な `main`。

読み方（**active / planned / 不能 / 不採用**）は [ARCHITECTURE_CONSTITUTION.md](ARCHITECTURE_CONSTITUTION.md) 第 0 節と同じ。

---

### GIT-001 — すべての変更は 1 つの Issue から始まる

Issue には問題と根拠・意図する結果・影響する規則 ID とモジュール・受け入れ条件・検証の計画・**やらないこと**を書く。
読み取りだけの調査は Issue 無しでよい。リポジトリの初期化コミットと Issue #1 だけが例外。

- 機械強制: **planned**（PR テンプレートの `Closes #N` を CI が検査する）
- 機械強制: **不能**（Issue の中身の妥当性）

### GIT-002 — ブランチ名は一つの形

```text
<type>/<issue番号>-<short-kebab-summary>
```

例: `feat/12-loupe-sampling`。`main` での直接開発・force push・既定ブランチの削除は禁止（初期化の例外を除く）。

- 機械強制: **planned**（ruleset で `main` を保護。ブランチ名の形は CI が PR の head で検査する）

### GIT-003 — コミットメッセージは一つの形

Conventional Commits を、この正確な形で書く。

```text
<type>(<scope>): <日本語の説明> (#<issue番号>)
```

- `type` は `feat` / `fix` / `docs` / `refactor` / `test` / `build` / `ci` / `chore` のいずれか
- `type`・`scope`（小文字・省略可）・`BREAKING CHANGE` 等の Conventional Commits の語は英語。**説明と本文は日本語**
- 件名は 100 文字以内。末尾に `(#N)` を必ず付ける
- 互換性を壊す公開契約の変更は `:` の前に `!` を付け、フッタに `BREAKING CHANGE:` を書く
- 件名には「何を」ではなく**判断**を置く（例: `版は rust-toolchain.toml が正で、CI には書かない`）

- 機械強制: **planned**（`.githooks/commit-msg` → `eng/validate-commit-message.ps1`。参照実装は NeNeCommander）
- 補足: squash merge の件名も同じ形。**マージ時に人が書き換える件名が最後の穴**なので、PR タイトルを同じ形に揃え、CI が PR タイトルも検査する

### GIT-004 — PR が統合の境界

PR には目的・変更の要約・使った正典経路・規則 ID・検証結果・waiver・残るリスク・`Closes #N` を書く（DEVELOPMENT_WORKFLOW 第 7 節）。
1 つの PR に 1 つの作業単位。マージは squash のみ。マージ後はローカル `main` を綺麗に同期する。

実装中とレビュー中は draft。必要な差分検証と結果・再利用の根拠がそろったら Ready にする（QLT-001 / QLT-012）。
push・レビュー・merge・担当変更・文書追記・SHA 変更だけでは成功済み検証を繰り返さない。
CI の必須 `check` は Git 規約・検証記録の存在・差分の空白を確認する。記録の妥当性はレビューし、CI 成功を製品テストの実行証拠とは扱わない。

- 機械強制: **planned**（PR テンプレート・必須 check・ruleset の squash-only）

---

## フックの導入

フックはリポジトリが所有する（`.githooks/`）。clone 直後に 1 回、次を実行する。

```powershell
pwsh -NoProfile -File ./eng/bootstrap.ps1     # core.hooksPath=.githooks を設定し、道具の版を確認する
```

`bootstrap.ps1` はグローバルの git 設定を変えない。既存の `pre-commit` は staged 差分の空白、`commit-msg` はメッセージだけを確認する。
push / merge フックは置かない。フックで指定なしの全件検証や成功済みテストの再実行を強制しない。
必要な挙動の検証は差分から選んで PR に記録する。フック成功だけで検証済みとは扱わない。

🔴 フックはローカルでしか動かない。`--no-verify` で迂回できるので、**CI が PR の全コミットと PR タイトルを同じ検証器で検査する**ことで初めて active と書ける。
