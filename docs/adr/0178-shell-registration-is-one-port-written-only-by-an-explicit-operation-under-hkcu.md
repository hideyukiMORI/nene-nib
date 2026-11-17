# ADR 0178 — Windows のエクスプローラーへの登録は port 1 本で、HKCU だけに、明示の操作のときだけ書く

- 状態: 受理（施主決定 D73・2026-10-29。段 1 の実装の時期は hide の判断・受理は D41 の形）
- 日付: 2026-10-30
- Issue: #1087
- 規則: FR-033、ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-011 / CPP-013、ADR 0078（操作の表）/ 0123（表示言語）/ 0130（DLL の遅延読み込み）/ 0137（静かな道）/ 0154（文書の言語の表）/ 0175（port 1 本の形）

## 文脈

Nib は zip で配る exe で、インストーラーが無いので OS に登録されていない。エクスプローラーでファイルを右クリックしても「NeNe Nib で開く」が無く、「プログラムから開く」の一覧にも「既定のアプリ」にも出ない（メモ帳は出る・外からの講評）。hide は D73 で「入れる」と決め、段 1（管理者権限なし・HKCU・F1 の操作）は実装予定、段 2（署名とパッケージの identity・Windows 11 の最上位のメニュー）は Microsoft Store のリリースの段階と決めた。

レジストリに触れられるのは `src/adapters/win32` だけ（ARC-007・リンカのシンボル）。登録は OS の状態を変える操作なので、起動や保存の道で黙って書いてはならない（ADR 0137 の静かな道と同じ考え）。

## 決定

1. **port は 1 本。** `application::ShellRegistrationPort` に 4 つ: `[[nodiscard]] virtual std::expected<std::string, ShellRegistrationFailure> executable_path() = 0;`（自分の exe のパス・UTF-8・adapter は `GetModuleFileNameW`・controller は操作のときだけ呼ぶ・起動では呼ばない・#1174 の席が見つけた穴）・ `[[nodiscard]] virtual std::expected<void, ShellRegistrationFailure> apply(const core::ShellRegistrationPlan &) = 0;`（鍵と値を書く）・`[[nodiscard]] virtual std::expected<void, ShellRegistrationFailure> remove(const core::ShellRegistrationPlan &) = 0;`（同じ鍵を消す）・`[[nodiscard]] virtual ShellRegistrationStatus status(const core::ShellRegistrationPlan &) = 0;`（`registered` / `registered_elsewhere`（command の exe のパスが自分と違う）/ `absent`）。`FolderPort` と同じ 5 つの削除と仮想デストラクタ。`EditorPorts` に `ShellRegistrationPort &shell_registration`。失敗は閉じた enum `ShellRegistrationFailure{access_denied, io_error}`（例外にしない・ARC-010）。
   - adapter は `Win32ShellRegistration`（`RegCreateKeyExW` / `RegSetValueExW` / `RegDeleteTreeW` / `RegOpenKeyExW` / `RegQueryValueExW`・`GetModuleFileNameW` で自分の exe のパス・書いた後に `SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST)`）。advapi32 と shell32 は起動の道で既に読み込まれる（ADR 0130 決定 2: `CommandLineToArgvW` と OS のテーマの `RegGetValueW`）ので遅延読み込みにはしない（費用は増えない）。起動で port を呼ばないことは `verify_quiet_paths` が守る（#1185 の席の判断 A）。
   - adapter は根の副鍵を 1 つ受け取る（製品は `Software`・試験は `Software\NeNeNib\test-<pid>`）。試験は実機の HKCU のその根の下にだけ書いて消す。替え玉は `ScriptedShellRegistration`（呼ばれた回数と最後の計画を覚える）。

2. **鍵と値は core の純関数が作る。** `core::shell_registration_plan(ShellRegistrationInput{exe_path, display_name, friendly_app_name, extensions}) -> ShellRegistrationPlan`（`std::vector<ShellRegistrationEntry{key_path, value_name, value, removal}>`・全部 UTF-8 の文字列・書く順。`removal` は閉じた enum `ShellRegistrationRemoval{subtree, value}`: 自分の木（`Classes\*\shell\NeNeNib`・`Classes\Applications\NeNeNib.exe`・`Classes\NeNeNib.<ext>`・`Software\NeNeNib`）は `remove` が木ごと消し、共有の `RegisteredApplications` の値は値だけ消す）。ProgId は `NeNeNib.<ext>`（`.` を除く）・`ApplicationDescription` は `friendly_app_name` と同じ・表示名は表示言語の表（#1019）が入るまで controller のリテラル。
   - `Software\Classes\*\shell\NeNeNib`: `(Default)` = 表示名「NeNe Nib で開く」（表示言語の表 ADR 0123 の 1 行）・`Icon` = `"<exe>",0`・`command\(Default)` = `"<exe>" "%1"`。
   - `Software\Classes\Applications\NeNeNib.exe`: `FriendlyAppName`・`shell\open\command\(Default)` = `"<exe>" "%1"`・`SupportedTypes\<ext>` = 空（「プログラムから開く」に出る）。
   - `Software\Classes\NeNeNib.<ext>`（ProgId・拡張子ごと）: `(Default)` = 「<ext> ファイル（NeNe Nib）」・`DefaultIcon\(Default)` = `"<exe>",0`・`shell\open\command\(Default)` = `"<exe>" "%1"`。
   - `Software\NeNeNib\Capabilities`: `ApplicationName`・`ApplicationDescription`・`FileAssociations\<ext>` = `NeNeNib.<ext>`。`Software\RegisteredApplications\NeNeNib` = `Software\NeNeNib\Capabilities`（設定アプリの「既定のアプリ」に出る）。
   - 拡張子は文書の言語の表（ADR 0154・`LanguageTable`）の拡張子の全部と `.txt`（重複は 1 つ・小文字・`.` 付き）。別の表は書かない。
   - パスは `"` で囲み、パスの中の `"` と空のパスは拒む（`std::expected` の失敗・zip で配る exe の置き場に `"` は無い）。`%1` も `"` で囲む（空白を含むパスのため）。

3. **操作は 2 つ、鍵なし、両モード。** `EditorOperation::register_shell`（「Windows の右クリックに登録する」）と `unregister_shell`（「登録を外す」）。F1 の一覧（Ctrl+P の `?`）と設定モーダル（#1016）の行から実行。実行は `run_operation` の 1 本 → 意図 `RegisterShell` / `UnregisterShell`（空の struct・使う人の操作）。controller は `ports_.shell_registration` を**このときだけ**呼び、結果を 1 行知らせる（「Windows の右クリックに登録しました」「登録を外しました」「登録されていません」「登録できませんでした: <失敗の種類>」・鳴らさない）。`unregister_shell` は `status` が `absent` なら書かずに知らせる。

4. **起動には載せない。** 起動・開く・切り替え・保存・打鍵・自動退避・F1 の一覧を開くときに port を呼ばない（契約 `verify_quiet_paths` に回数の欄）。exe の置き場が変わって登録が古くなった場合（`registered_elsewhere`）は、起動で検出せず、使う人が「登録する」を実行したときに上書きで直す（知らせの文に「以前の登録を置き換えました」）。

5. **Windows 11 の最上位のメニュー（段 2）。** 段 1 の登録は Windows 11 では「その他のオプションを表示」の中に出る。最上位に出すにはパッケージの identity（MSIX か sparse package）と `IExplorerCommand` の COM サーバーと署名が要り、Store のリリースの段階で別の ADR にする（D73）。段 1 の知らせの文にこの事実を 1 句添える（「Windows 11 では『その他のオプションを表示』の中に出ます」）。

6. **契約。** core `--shell-registration`（計画の鍵と値・パスの引用符・`%1`・拡張子の重複と小文字・`"` を含むパスの拒否）。application `--shell-registration`（意図 → port 1 回・成功と失敗の知らせ・`absent` の `unregister`・`registered_elsewhere` の上書き・`verify_quiet_paths` の 0 回）。adapters `nib_adapter_tests`（試験の根の下に書く → `status` → 消す → `absent`・`RegDeleteTreeW` で残骸 0）。`eng/symbols.py` は `Reg*` と `SHChangeNotify` を adapters の外で拒む（ARC-007・既存の仕組み・許可の表に足さない）。速さ: 打鍵と起動の道に載らない（port は操作のときだけ）。

7. **限界（後続）。** 「送る」メニュー・ファイルの種類ごとの既定の設定の UI からの誘導（`ms-settings:defaultapps`）・登録の状態を設定モーダルに出す（#1016 の表の 1 行・そのときは開いたときに `status` を 1 回読む）・段 2 は後続。

## 強制

- レジストリと shell の API が adapters だけ: **active**（`eng/symbols.py`・ARC-007）。
- 登録の鍵と値が core の純関数 1 本: **planned**（契約 `--shell-registration`・#1087 の PR の merge で active にする）。
- 起動と静かな道で port を呼ばない: **planned**（`verify_quiet_paths` の欄・同じ PR で active にする）。

## 結果

得られるもの: メモ帳と同じく右クリックと「プログラムから開く」と「既定のアプリ」から Nib を選べる。登録は HKCU だけで管理者権限が要らず、消すのも同じ操作。鍵と値は純関数なので試験は文字列で確かめられる。
失うもの・残る穴: Windows 11 の最上位のメニューには出ない（段 2）。exe を動かすと登録が古くなり、直すのは使う人の操作（起動では直さない）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 起動のたびに登録を確かめて直す | OS の状態を黙って書く。静かな道（ADR 0137）と D73「明示の操作でだけ書く」に反する |
| HKLM に書く（全ユーザー） | 管理者権限が要り、zip で配る exe の段階に合わない。段 2（インストーラー・Store）で決める |
| `IExplorerCommand` の COM サーバーを段 1 で書く | パッケージの identity と署名が無いと Windows 11 の最上位に出ない。費用に見合わない（D73） |
| 鍵と値を adapter が直に組み立てる | 試験が実機のレジストリに依る。純関数なら文字列で固定できる（ARC-003） |

## 追補（2026-11-17・#1809）: Store 版（パッケージ）では 2 つの操作を F1 の一覧に出さない

Store 版（MSIX）ではエクスプローラーに項目が出ない（1.0.0 の施主の確かめ 6-C-4 で確かめた）。理由は、HKCU の書き込みがパッケージ専用の場所へ写るためと見ている（読み・写り先は確かめていない）。効かない操作を一覧に置かない。ZIP 版の振る舞いは変えない。「プログラムから開く」をマニフェストで宣言する道は別の Issue。

1. **製品の形は閉じた値 `core::Distribution{unpackaged, packaged}`。** OS に問うのは `src/adapters/win32/Win32Distribution` の `current_distribution()` だけで（ARC-007）、`GetCurrentPackageFullName` が名前の置き場が足りない（`ERROR_INSUFFICIENT_BUFFER`）か成功を返せば package identity が在るので `packaged`、`APPMODEL_ERROR_NO_PACKAGE` と想定外の値は `unpackaged`（写しは純関数 `distribution_of`）。分からないときは今までどおり（ZIP 版）に倒す。誤って `packaged` にすると ZIP 版で効く機能が消えるが、誤って `unpackaged` にしても 1.0.0 と同じ振る舞いに戻るだけ（独立レビュー V-1814）。合成ルートが起動時に 1 回だけ呼び、`EditorController` の構築の引数に渡す。core と application は値を受けるだけ。
2. **どの操作がその形で効くかは core の `operation_distributed(operation, distribution)` の 1 本。** `packaged` では `register_shell` と `unregister_shell` が偽。F1 の一覧（`operation_choices`）は `PaletteMode.distribution` でこれを読んで行を外す。controller は面を開くとき `PaletteMode{mode, menu, distribution_}` を名指しで作る（`palette_mode_of(EditMode)` は編集モードだけを知る試験と測定の口で ZIP 版）。
3. **別の道で届いても書かない。** 鍵・古い一覧の位置などから `RegisterShell` / `UnregisterShell` が届いたら、`packaged` では port を呼ばず、鳴らさない 1 行（`UiMessage::notice_shell_packaged`・3 列）で知らせる。
4. **契約。** core `--operations`: 一覧の件数をパッケージの有無で 2 通り（通常・Vim の INSERT・NORMAL・VISUAL）、`packaged` の列は 2 つを除いて表の順。application `--shell-registration`: `packaged` の controller で一覧に 2 つが無く、2 つの意図が port に触れずに知らせる（パッケージの状態は値を渡すだけで作り、OS を偽装しない）。adapters: 写しの 4 値（想定外の値は `unpackaged`）と、試験の exe（ZIP の形）で `unpackaged`。表の契約（`verify_no_shared_key` ほか）は不変。
